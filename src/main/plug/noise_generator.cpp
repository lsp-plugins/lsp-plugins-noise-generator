/*
 * Copyright (C) 2022 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2022 Stefano Tronci <stefano.tronci@protonmail.com>
 *
 * This file is part of lsp-plugins
 * Created on: 27 Feb 2022
 *
 * lsp-plugins is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins. If not, see <https://www.gnu.org/licenses/>.
 */

#include <lsp-plug.in/common/alloc.h>
#include <lsp-plug.in/common/debug.h>
#include <lsp-plug.in/dsp/dsp.h>
#include <lsp-plug.in/dsp-units/units.h>
#include <lsp-plug.in/plug-fw/meta/func.h>
#include <lsp-plug.in/runtime/system.h>
#include <lsp-plug.in/shared/id_colors.h>

#include <private/meta/noise_generator.h>
#include <private/plugins/noise_generator.h>

/* The size of temporary buffer for audio processing */
#define BUFFER_SIZE                 0x1000U
#define INA_FILTER_ORD              64
#define INA_FILTER_CUTOFF           (DEFAULT_SAMPLE_RATE * 0.5f)
#define INA_ATTENUATION             0.5f // We attenuate the noise before filtering to make it inaudible. This to prevent sharp transients from still being audible.
#define COLOR_FILTER_ORDER          32
#define IDISPLAY_BUF_SIZE           64u  // Number of samples in frequency chart for Inline Display

namespace lsp
{
    inline plug::IPort *TRACE_PORT(plug::IPort *p)
    {
        lsp_trace("  port id=%s", p->metadata()->id);
        return p;
    }

    namespace plugins
    {
        //---------------------------------------------------------------------
        // Plugin factory
        static const meta::plugin_t *plugins[] =
        {
            &meta::noise_generator_x1,
            &meta::noise_generator_x2,
			&meta::noise_generator_x4,
        };

        static plug::Module *plugin_factory(const meta::plugin_t *meta)
        {
            return new noise_generator(meta);
        }

        static plug::Factory factory(plugin_factory, plugins, 3);

        //---------------------------------------------------------------------
        // Implementation
        noise_generator::noise_generator(const meta::plugin_t *meta):
            Module(meta)
        {
            // Compute the number of audio channels by the number of inputs
            nChannels       = 0;
            for (const meta::port_t *p = meta->ports; p->id != NULL; ++p)
                if (meta::is_audio_in_port(p))
                    ++nChannels;

            // Initialize other parameters
            vChannels       = NULL;
            vBuffer         = NULL;
            vTemp           = NULL;
            vFreqs          = NULL;
            vFreqChart      = NULL;
            fGainIn         = GAIN_AMP_0_DB;
            fGainOut        = GAIN_AMP_0_DB;
            pData           = NULL;
            pIDisplay       = NULL;

            pBypass         = NULL;
            pGainIn         = NULL;
            pGainOut        = NULL;
        }

        noise_generator::~noise_generator()
        {
            destroy();
        }

        ssize_t noise_generator::make_seed() const
        {
            system::time_t ts;
            system::get_time(&ts);
            return ts.seconds ^ ts.nanos;
        }

        dspu::lcg_dist_t noise_generator::get_lcg_dist(size_t value)
        {
            switch (value)
            {
                case meta::noise_generator_metadata::NOISE_LCG_UNIFORM:
                    return dspu::LCG_UNIFORM;
                case meta::noise_generator_metadata::NOISE_LCG_EXPONENTIAL:
                    return dspu::LCG_EXPONENTIAL;
                case meta::noise_generator_metadata::NOISE_LCG_TRIANGULAR:
                    return dspu::LCG_TRIANGULAR;
                case meta::noise_generator_metadata::NOISE_LCG_GAUSSIAN:
                default:
                    return dspu::LCG_GAUSSIAN;
            }
        }

        dspu::vn_velvet_type_t noise_generator::get_velvet_type(size_t value)
        {
            switch (value)
            {
                case meta::noise_generator_metadata::NOISE_VELVET_OVNA:
                    return dspu::VN_VELVET_OVNA;
                case meta::noise_generator_metadata::NOISE_VELVET_ARN:
                    return dspu::VN_VELVET_ARN;
                case meta::noise_generator_metadata::NOISE_VELVET_TRN:
                    return dspu::VN_VELVET_TRN;
                case meta::noise_generator_metadata::NOISE_VELVET_OVN:
                default:
                    return dspu::VN_VELVET_OVN;
            }
        }

        dspu::ng_color_t noise_generator::get_color(size_t value)
        {
            switch (value)
            {
                case meta::noise_generator_metadata::NOISE_COLOR_PINK:
                    return dspu::NG_COLOR_PINK;
                case meta::noise_generator_metadata::NOISE_COLOR_RED:
                    return dspu::NG_COLOR_RED;
                case meta::noise_generator_metadata::NOISE_COLOR_BLUE:
                    return dspu::NG_COLOR_BLUE;
                case meta::noise_generator_metadata::NOISE_COLOR_VIOLET:
                    return dspu::NG_COLOR_VIOLET;
                case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_NPN:
                case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_DBO:
                case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_DBD:
                    return dspu::NG_COLOR_ARBITRARY;
                case meta::noise_generator_metadata::NOISE_COLOR_WHITE:
                default:
                    return dspu::NG_COLOR_WHITE;
            }
        }

        dspu::stlt_slope_unit_t noise_generator::get_color_slope_unit(size_t value)
        {
            switch (value)
            {
                case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_DBO:
                    return dspu::STLT_SLOPE_UNIT_DB_PER_OCTAVE;
                case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_DBD:
                    return dspu::STLT_SLOPE_UNIT_DB_PER_DECADE;
                case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_NPN:
                default:
                    return dspu::STLT_SLOPE_UNIT_NEPER_PER_NEPER;
            }
        }

        noise_generator::ch_mode_t noise_generator::get_channel_mode(size_t value)
        {
            switch (value)
            {
                case meta::noise_generator_metadata::CHANNEL_MODE_ADD:
                    return CH_MODE_ADD;
                case meta::noise_generator_metadata::CHANNEL_MODE_MULT:
                    return CH_MODE_MULT;
                case meta::noise_generator_metadata::CHANNEL_MODE_OVERWRITE:
                default:
                    return CH_MODE_OVERWRITE;
            }
        }

        void noise_generator::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialisation
            Module::init(wrapper, ports);

            // Estimate the number of bytes to allocate
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);

            /** Buffers:
             * 2X Temporary Buffer for Processing (BUFFER_SIZE)
             * 1X Frequency List (MESH_POINTS)
             * 1X Complex Part of Frequency Response (MESH_POINTS)
             * 1X Frequency Chart of Channel (MESH_POINTS)
             */
            size_t buf_sz           = align_size(BUFFER_SIZE * sizeof(float), OPTIMAL_ALIGN);
            size_t chr_sz           = align_size(meta::noise_generator::MESH_POINTS *  sizeof(float), OPTIMAL_ALIGN);
            size_t gen_sz           = (chr_sz + buf_sz) * meta::noise_generator::NUM_GENERATORS;
            size_t alloc            = szof_channels + // vChannels
                                      2*buf_sz + // vBuffer, vTemp
                                      3*chr_sz + // vFreqs, vFreqChar(2)
                                      gen_sz; // vGenerators[i].vFreqChart

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialise pointers to channels and temporary buffer
            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;
            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vTemp                   = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vFreqs                  = reinterpret_cast<float *>(ptr);
            ptr                    += chr_sz;
            vFreqChart              = reinterpret_cast<float *>(ptr);
            ptr                    += chr_sz * 2;

            // Initialize generators
            for (size_t i=0; i<meta::noise_generator::NUM_GENERATORS; ++i)
            {
                generator_t *g          = &vGenerators[i];

                // Construct in-place DSP processors
                g->sNoiseGenerator.construct();
                g->sAudibleStop.construct();
                g->sAudibleStop.init();

                // We seed every noise generator differently so that they produce uncorrelated noise.
                // We set the MLS number of bits to -1 so that the initialiser sets it to maximum.
                g->sNoiseGenerator.init(
                    -1, make_seed(),
                    make_seed(),
                    make_seed(), -1, make_seed());

                // We also set the inaudible noise filter main properties. These are not user configurable.
                g->sAudibleStop.set_order(INA_FILTER_ORD);
                g->sAudibleStop.set_filter_type(dspu::BW_FLT_TYPE_HIGHPASS);

                // Same with colour
                g->sNoiseGenerator.set_coloring_order(COLOR_FILTER_ORDER);

                // Initialize settings
                g->fGain                = GAIN_AMP_0_DB;
                g->bActive              = false;
                g->bInaudible           = false;
                g->bUpdPlots            = true;

                g->vBuffer              = reinterpret_cast<float *>(ptr);
                ptr                    += buf_sz;
                g->vFreqChart           = reinterpret_cast<float *>(ptr);
                ptr                    += chr_sz;

                // Initialize input ports
                g->pNoiseType           = NULL;
                g->pAmplitude           = NULL;
                g->pOffset              = NULL;
                g->pSlSw                = NULL;
                g->pMtSw                = NULL;
                g->pInaSw               = NULL;
                g->pLCGdist             = NULL;
                g->pVelvetType          = NULL;
                g->pVelvetWin           = NULL;
                g->pVelvetARNd          = NULL;
                g->pVelvetCSW           = NULL;
                g->pVelvetCpr           = NULL;
                g->pColorSel            = NULL;
                g->pCslopeNPN           = NULL;
                g->pCslopeDBO           = NULL;
                g->pCslopeDBD           = NULL;
                g->pMeterOut            = NULL;
                g->pMsh                 = NULL;
            }

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();

                // Initialise fields
                c->enMode 				= CH_MODE_OVERWRITE;
                for (size_t j=0; j < meta::noise_generator::NUM_GENERATORS; ++j)
                    c->vGain[j]             = GAIN_AMP_0_DB;
                c->fGainOut             = GAIN_AMP_0_DB;
                c->bActive              = true;

                // Initialize ports
                c->pIn                  = NULL;
                c->pOut                 = NULL;
                c->pSlSw                = NULL;
                c->pMtSw                = NULL;
                c->pNoiseMode           = NULL;
                for (size_t j=0; j < meta::noise_generator::NUM_GENERATORS; ++j)
                    c->pGain[j]             = NULL;
                c->pGainOut             = NULL;
                c->pMeterIn             = NULL;
                c->pMeterOut            = NULL;
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            lsp_trace("Binding audio ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];
                c->pIn                  = TRACE_PORT(ports[port_id++]);
                c->pOut                 = TRACE_PORT(ports[port_id++]);
            }

            // Bind global ports
            lsp_trace("Binding global control ports");
            pBypass                     = TRACE_PORT(ports[port_id++]);
            pGainIn                     = TRACE_PORT(ports[port_id++]);
            pGainOut                    = TRACE_PORT(ports[port_id++]);
            TRACE_PORT(ports[port_id++]);   // Skip 'Zoom' control

            // Bind generator ports
            lsp_trace("Binding generator ports");
            for (size_t i=0; i<meta::noise_generator::NUM_GENERATORS; ++i)
            {
                generator_t *g          = &vGenerators[i];

                g->pNoiseType           = TRACE_PORT(ports[port_id++]);
                g->pAmplitude           = TRACE_PORT(ports[port_id++]);
                g->pOffset              = TRACE_PORT(ports[port_id++]);
                g->pSlSw                = TRACE_PORT(ports[port_id++]);
                g->pMtSw                = TRACE_PORT(ports[port_id++]);
                g->pInaSw               = TRACE_PORT(ports[port_id++]);

                g->pLCGdist             = TRACE_PORT(ports[port_id++]);

                g->pVelvetType          = TRACE_PORT(ports[port_id++]);
                g->pVelvetWin           = TRACE_PORT(ports[port_id++]);
                g->pVelvetARNd          = TRACE_PORT(ports[port_id++]);
                g->pVelvetCSW           = TRACE_PORT(ports[port_id++]);
                g->pVelvetCpr           = TRACE_PORT(ports[port_id++]);

                g->pColorSel            = TRACE_PORT(ports[port_id++]);
                g->pCslopeNPN           = TRACE_PORT(ports[port_id++]);
                g->pCslopeDBO           = TRACE_PORT(ports[port_id++]);
                g->pCslopeDBD           = TRACE_PORT(ports[port_id++]);

                g->pMeterOut            = TRACE_PORT(ports[port_id++]);
                g->pMsh                 = TRACE_PORT(ports[port_id++]);
            }

            // Bind channel control ports
            lsp_trace("Binding channel control ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                if (nChannels > 1)
                {
                    c->pSlSw                = TRACE_PORT(ports[port_id++]);
                    c->pMtSw                = TRACE_PORT(ports[port_id++]);
                }
                c->pNoiseMode 	        = TRACE_PORT(ports[port_id++]);
                for (size_t j=0; j<meta::noise_generator::NUM_GENERATORS; ++j)
                    c->pGain[j]             = TRACE_PORT(ports[port_id++]);
                c->pGainOut             = TRACE_PORT(ports[port_id++]);
                c->pMeterIn             = TRACE_PORT(ports[port_id++]);
                c->pMeterOut            = TRACE_PORT(ports[port_id++]);
            }
        }

        void noise_generator::destroy()
        {
            // Drop inline display data structures
            if (pIDisplay != NULL)
            {
                pIDisplay->destroy();
                pIDisplay   = NULL;
            }

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sBypass.destroy();
                }
                vChannels = NULL;
            }

            // Destroy noise generators
            for (size_t i=0; i<meta::noise_generator::NUM_GENERATORS; ++i)
            {
                generator_t *g  = &vGenerators[i];
                g->vFreqChart   = NULL;
                g->sNoiseGenerator.destroy();
                g->sAudibleStop.destroy();
            }

            // Forget about buffers
            vBuffer     = NULL;
            vTemp       = NULL;
            vFreqs      = NULL;
            vFreqChart  = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }

            // Destroy parent module
            Module::destroy();
        }

        void noise_generator::update_sample_rate(long sr)
        {
            // Initialize list of frequencies
            constexpr float min_freq    = SPEC_FREQ_MIN;
            float max_freq              = lsp_min(sr * 0.5f, SPEC_FREQ_MAX);
            float norm                  = logf(max_freq/min_freq) / (meta::noise_generator_metadata::MESH_POINTS - 1);
            for (size_t i=0; i<meta::noise_generator_metadata::MESH_POINTS; ++i)
                vFreqs[i]                   = min_freq * expf(i * norm);

            // Update sample rate for channel processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->sBypass.init(sr);
            }

            // Update sample rate for generators
            for (size_t i=0; i<meta::noise_generator::NUM_GENERATORS; ++i)
            {
                generator_t *g  = &vGenerators[i];
                g->sNoiseGenerator.set_sample_rate(sr);
                g->sAudibleStop.set_sample_rate(sr);
                g->sAudibleStop.set_cutoff_frequency(INA_FILTER_CUTOFF);
            }
        }

        void noise_generator::update_settings()
        {
            // Use if the sample rate does not allow actual inaudible noise
            bool force_audible  = (0.5f * fSampleRate) < INA_FILTER_CUTOFF;
            bool bypass         = pBypass->value() >= 0.5f;

            // Check if one of the channels is solo.
            bool g_has_solo     = false;
            bool c_has_solo     = false;

            // Search for soloing channels
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                if ((c->pSlSw != NULL) && (c->pSlSw->value() >= 0.5f))
                {
                    c_has_solo  = true;
                    break;
                }
            }

            // Search for soloing generators
            for (size_t i=0; i<meta::noise_generator_metadata::NUM_GENERATORS; ++i)
            {
                generator_t *g      = &vGenerators[i];
                if ((g->pSlSw != NULL) && (g->pSlSw->value() >= 0.5f))
                {
                    g_has_solo  = true;
                    break;
                }
            }

            // Update the configuration of each output channel
            fGainIn                 = pGainIn->value();
            fGainOut                = pGainOut->value();

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];
                bool solo               = (c->pSlSw != NULL) ? c->pSlSw->value() >= 0.5f : false;
                bool mute               = (c->pMtSw != NULL) ? c->pMtSw->value() >= 0.5f : false;

                c->enMode               = get_channel_mode(c->pNoiseMode->value());
                for (size_t j=0; j<meta::noise_generator_metadata::NUM_GENERATORS; ++j)
                    c->vGain[j]             = c->pGain[j]->value();
                c->fGainOut             = c->pGainOut->value();
                c->bActive              = (c_has_solo) ? solo : !mute;

                // Update bypass
                c->sBypass.set_bypass(bypass);
            }

            for (size_t i=0; i<meta::noise_generator_metadata::NUM_GENERATORS; ++i)
            {
                generator_t *g          = &vGenerators[i];

                // If one of the channels is solo, then we simply know from the solo switch if this channel
                // is active. Otherwise, we check whether the channel was set to mute or not.
                bool solo               = (g->pSlSw != NULL) ? g->pSlSw->value() >= 0.5f : false;
                bool mute               = (g->pMtSw != NULL) ? g->pMtSw->value() >= 0.5f : false;
                g->bActive              = (g_has_solo) ? solo : !mute;
                g->bInaudible           = (force_audible) ? false : g->pInaSw->value() >= 0.5f;

                // Configure noise generator
                dspu::lcg_dist_t lcgdist = get_lcg_dist(g->pLCGdist->value());
                dspu::vn_velvet_type_t velvettype = get_velvet_type(g->pVelvetType->value());
                bool velvetcs           = g->pVelvetCSW->value() >= 0.5f;
                float velvetcsp         = g->pVelvetCpr->value() * 0.01f;
                dspu::ng_color_t color  = (g->bInaudible) ? dspu::NG_COLOR_WHITE : get_color(g->pColorSel->value());
                dspu::stlt_slope_unit_t color_slope_unit    = get_color_slope_unit(g->pColorSel->value());

                float color_slope       = -0.5f;
                switch (color_slope_unit)
                {
                    case dspu::STLT_SLOPE_UNIT_DB_PER_OCTAVE:
                        color_slope         = g->pCslopeDBO->value();
                        break;

                    case dspu::STLT_SLOPE_UNIT_DB_PER_DECADE:
                        color_slope         = g->pCslopeDBD->value();
                        break;

                    case dspu::STLT_SLOPE_UNIT_NEPER_PER_NEPER:
                    default:
                        color_slope         = g->pCslopeNPN->value();
                        break;
                }

                // If the noise has to be inaudible we are best setting it to white, or excessive high frequency boost will make it audible.
                // Conversely, excessive low frequency attenuation will make it non-existent.
                g->sNoiseGenerator.set_lcg_distribution(lcgdist);
                g->sNoiseGenerator.set_velvet_type(velvettype);
                g->sNoiseGenerator.set_velvet_window_width(g->pVelvetWin->value());
                g->sNoiseGenerator.set_velvet_arn_delta(g->pVelvetARNd->value());
                g->sNoiseGenerator.set_velvet_crush(velvetcs);
                g->sNoiseGenerator.set_velvet_crushing_probability(velvetcsp);
                g->sNoiseGenerator.set_noise_color(color);
                g->sNoiseGenerator.set_color_slope(color_slope, color_slope_unit);
                g->sNoiseGenerator.set_amplitude(g->pAmplitude->value());
                g->sNoiseGenerator.set_offset(g->pOffset->value());

                size_t noise_type = g->pNoiseType->value();
                switch (noise_type)
                {
                    case meta::noise_generator_metadata::NOISE_TYPE_MLS:
                        g->sNoiseGenerator.set_generator(dspu::NG_GEN_MLS);
                        break;
                    case meta::noise_generator_metadata::NOISE_TYPE_VELVET:
                        g->sNoiseGenerator.set_generator(dspu::NG_GEN_VELVET);
                        break;
                    case meta::noise_generator_metadata::NOISE_TYPE_LCG:
                        g->sNoiseGenerator.set_generator(dspu::NG_GEN_LCG);
                        break;
                    case meta::noise_generator_metadata::NOISE_TYPE_OFF:
                    default:
                        g->sNoiseGenerator.set_generator(dspu::NG_GEN_LCG);
                        g->bActive          = false;
                        break;
                }

                // Plots only really need update when we operate the controls, se we set the update to true
                g->bUpdPlots        = true;
        	}

            // Query inline display redraw
            pWrapper->query_display_draw();
        }

        void noise_generator::process(size_t samples)
        {
            // Initialize buffer pointers
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];
                c->vIn                  = c->pIn->buffer<float>();
                c->vOut                 = c->pOut->buffer<float>();
            }
            lsp_finally
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c            = &vChannels[i];
                    c->vIn                  = NULL;
                    c->vOut                 = NULL;
                }
            };

            // Process data
            for (size_t count = samples; count > 0;)
            {
                size_t to_do    = lsp_min(count, BUFFER_SIZE);

                // Run each noise generator first to generate random noise sequences
                for (size_t i=0; i<meta::noise_generator_metadata::NUM_GENERATORS; ++i)
                {
                    generator_t *g  = &vGenerators[i];
                    float level     = GAIN_AMP_M_INF_DB;

                    if (g->bActive)
                    {
                        g->sNoiseGenerator.process_overwrite(g->vBuffer, to_do);
                        if (g->bInaudible)
                        {
                            dsp::mul_k2(g->vBuffer, INA_ATTENUATION, to_do);
                            g->sAudibleStop.process_overwrite(g->vBuffer, g->vBuffer, to_do);
                        }
                        level           = dsp::abs_max(g->vBuffer, to_do);
                    }
                    else
                        dsp::fill_zero(g->vBuffer, to_do);

                    g->pMeterOut->set_value(level);
                }

                // Process each channel independently
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c            = &vChannels[i];

                    // Apply input gain and measure the input level
                    dsp::mul_k3(vTemp, c->vIn, fGainIn, to_do);
                    float level             = dsp::abs_max(vTemp, to_do);
                    c->pMeterIn->set_value(level);

                    // Apply matrix to the temporary buffer
                    dsp::fill_zero(vBuffer, to_do);
                    if (c->bActive)
                    {
                        // Apply gain of each generator to the output buffer
                        for (size_t j=0; j<meta::noise_generator_metadata::NUM_GENERATORS; ++j)
                        {
                            generator_t *g      = &vGenerators[j];
                            dsp::fmadd_k3(vBuffer, g->vBuffer, c->vGain[j] * c->fGainOut, to_do);
                        }
                    }

                    // Now we have mixed output from generators, apply special mode to input
                    switch (c->enMode)
                    {
                        case CH_MODE_ADD:   dsp::fmadd_k3(vBuffer, vTemp, c->fGainOut, to_do); break;
                        case CH_MODE_MULT:  dsp::fmmul_k3(vBuffer, vTemp, c->fGainOut, to_do); break;
                        case CH_MODE_OVERWRITE:
                        default:
                            break;
                    }

                    // Apply output gain and measure output level
                    dsp::mul_k2(vBuffer, fGainOut, to_do);
                    level                   = dsp::abs_max(vBuffer, to_do);
                    c->pMeterOut->set_value(level);

                    // Post-process buffer
                    c->sBypass.process(c->vOut, c->vIn, vBuffer, to_do);

                    // Update pointers
                    c->vIn                 += to_do;
                    c->vOut                += to_do;
                }

                // Update counter
                count                  -= to_do;
            }

            // Process each generator independently
            for (size_t i=0; i<meta::noise_generator_metadata::NUM_GENERATORS; ++i)
            {
                generator_t *g      = &vGenerators[i];

                // Make a Frequency Chart - It only needs to be updated when the settings changed. so if bUpdPlots is true.
                // We do the chart after processing so that we chart the most up to date filter state.
                if (!g->bUpdPlots)
                    continue;

                plug::mesh_t *mesh = g->pMsh->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    // Compute frequency characteristics
                    if (g->bActive)
                    {
                        g->sNoiseGenerator.freq_chart(vFreqChart, vFreqs, meta::noise_generator_metadata::MESH_POINTS);
                        dsp::pcomplex_mod(g->vFreqChart, vFreqChart, meta::noise_generator_metadata::MESH_POINTS);
                    }
                    else
                        dsp::fill_zero(g->vFreqChart, meta::noise_generator_metadata::MESH_POINTS);

                    // Commit frequency characteristics to output mesh
                    dsp::copy(&mesh->pvData[0][2], vFreqs, meta::noise_generator_metadata::MESH_POINTS);
                    dsp::copy(&mesh->pvData[1][2], g->vFreqChart, meta::noise_generator_metadata::MESH_POINTS);

                    // Add extra points
                    mesh->pvData[0][0] = SPEC_FREQ_MIN*0.5f;
                    mesh->pvData[0][1] = SPEC_FREQ_MIN*0.5f;
                    mesh->pvData[0][meta::noise_generator_metadata::MESH_POINTS+2] = SPEC_FREQ_MAX*2.0f;
                    mesh->pvData[0][meta::noise_generator_metadata::MESH_POINTS+3] = SPEC_FREQ_MAX*2.0f;

                    mesh->pvData[1][0] = (g->bActive) ? GAIN_AMP_0_DB : 0.0f;
                    mesh->pvData[1][1] = g->vFreqChart[0];
                    mesh->pvData[1][meta::noise_generator_metadata::MESH_POINTS+2] = g->vFreqChart[meta::noise_generator_metadata::MESH_POINTS-1];
                    mesh->pvData[1][meta::noise_generator_metadata::MESH_POINTS+3] = mesh->pvData[1][0];

                    mesh->data(2, meta::noise_generator_metadata::MESH_POINTS + 4);

                    // Update state only
                    g->bUpdPlots = false;
                }
            } // for channels
        }

        bool noise_generator::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > width)
                height  = width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width       = cv->width();
            height      = cv->height();

            // Clear background
            bool bypassing = vChannels[0].sBypass.bypassing();
            cv->set_color_rgb((bypassing) ? CV_DISABLED : CV_BACKGROUND);
            cv->paint();

            // Draw axis
            cv->set_line_width(1.0);
            float zx    = 1.0f/SPEC_FREQ_MIN;
            float zy    = GAIN_AMP_P_48_DB;
            float dx    = width/logf(SPEC_FREQ_MAX/SPEC_FREQ_MIN);
            float dy    = height/logf(GAIN_AMP_M_48_DB/GAIN_AMP_P_48_DB);

            // Draw vertical lines
            cv->set_color_rgb(CV_YELLOW, 0.5f);
            for (float i=100.0f; i<SPEC_FREQ_MAX; i *= 10.0f)
            {
                float ax = dx*(logf(i*zx));
                cv->line(ax, 0, ax, height);
            }

            // Draw horizontal lines
            cv->set_color_rgb(CV_WHITE, 0.5f);
            for (float i=GAIN_AMP_M_48_DB; i<GAIN_AMP_P_48_DB; i *= GAIN_AMP_P_12_DB)
            {
                float ay = height + dy*(logf(i*zy));
                cv->line(0, ay, width, ay);
            }

            // Allocate buffer: f, amp, x, y
            pIDisplay           = core::IDBuffer::reuse(pIDisplay, 4, width+4);
            core::IDBuffer *b   = pIDisplay;
            if (b == NULL)
                return false;

            // Initialize mesh
            b->v[0][0]          = SPEC_FREQ_MIN*0.5f;
            b->v[0][1]          = SPEC_FREQ_MIN*0.5f;
            b->v[0][width+2]    = SPEC_FREQ_MAX*2.0f;
            b->v[0][width+3]    = SPEC_FREQ_MAX*2.0f;

            b->v[1][0]          = GAIN_AMP_0_DB;
            b->v[1][1]          = GAIN_AMP_0_DB;
            b->v[1][width+2]    = GAIN_AMP_0_DB;
            b->v[1][width+3]    = GAIN_AMP_0_DB;

            // Draw channels
            Color col(CV_MESH);
            bool aa = cv->set_anti_aliasing(true);
            lsp_finally { cv->set_anti_aliasing(aa); };
            cv->set_line_width(2);

            // Perform frequency decimation
            for (size_t j=0; j<width; ++j)
            {
                size_t k        = (j*meta::noise_generator_metadata::MESH_POINTS)/width;
                b->v[0][j+2]    = vFreqs[k];
            }
            dsp::fill_zero(b->v[2], width + 4);
            dsp::axis_apply_log1(b->v[2], b->v[0], zx, dx, width + 4);

            for (size_t i=0; i<meta::noise_generator_metadata::NUM_GENERATORS; ++i)
            {
                generator_t *g  = &vGenerators[i];
                if (!g->bActive)
                    continue;

                // Perform amplitude decimation
                for (size_t j=0; j<width; ++j)
                {
                    size_t k        = (j*meta::noise_generator_metadata::MESH_POINTS)/width;
                    b->v[1][j+2]    = g->vFreqChart[k];
                }
                b->v[1][1]      = b->v[1][2];
                b->v[1][width+2]= b->v[1][width+1];

                dsp::fill(b->v[3], height, width + 4);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width+4);

                // Draw mesh
                col.hue(float(i) / float(meta::noise_generator_metadata::NUM_GENERATORS));
                uint32_t color = (bypassing || !(active())) ? CV_SILVER : col.rgb24();
                Color stroke(color), fill(color, 0.5f);
                cv->draw_poly(b->v[2], b->v[3], width+4, stroke, fill);
            }

            return true;
        }

        void noise_generator::dump(dspu::IStateDumper *v) const
        {
            // Write generators
            v->begin_array("vGenerators", vGenerators, meta::noise_generator_metadata::NUM_GENERATORS);
            {
                // Write generators
                for (size_t i=0; i<meta::noise_generator_metadata::NUM_GENERATORS; ++i)
                {
                    const generator_t *g  = &vGenerators[i];

                    v->begin_object(g, sizeof(generator_t));
                    {
                        v->write_object("sNoiseGenerator", &g->sNoiseGenerator);
                        v->write_object("sAudibleStop", &g->sAudibleStop);

                        v->write("fGain", g->fGain);
                        v->write("bActive", g->bActive);
                        v->write("bInaudible", g->bInaudible);
                        v->write("bUpdPlots", g->bUpdPlots);

                        v->write("vBuffer", g->vBuffer);
                        v->write("vFreqChart", g->vFreqChart);

                        v->write("pNoiseType", g->pNoiseType);
                        v->write("pAmplitude", g->pAmplitude);
                        v->write("pOffset", g->pOffset);
                        v->write("pSlSw", g->pSlSw);
                        v->write("pMtSw", g->pMtSw);
                        v->write("pInaSw", g->pInaSw);
                        v->write("pLCGdist", g->pLCGdist);
                        v->write("pVelvetType", g->pVelvetType);
                        v->write("pVelvetWin", g->pVelvetWin);
                        v->write("pVelvetARNd", g->pVelvetARNd);
                        v->write("pVelvetCSW", g->pVelvetCSW);
                        v->write("pVelvetCpr", g->pVelvetCpr);
                        v->write("pColorSel", g->pColorSel);
                        v->write("pCslopeNPN", g->pCslopeNPN);
                        v->write("pCslopeDBO", g->pCslopeDBO);
                        v->write("pCslopeDBD", g->pCslopeDBD);
                        v->write("pMeterOut", g->pMeterOut);
                        v->write("pMsh", g->pMsh);
                    }
                    v->end_object();
                }
            }
            v->end_array();

            // It is very useful to dump plugin state for debug purposes
            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);
            {
                // Write channels
                for (size_t i=0; i<nChannels; ++i)
                {
                    const channel_t *c = &vChannels[i];

                    v->begin_object(c, sizeof(channel_t));
                    {
                        v->write("enMode", size_t(c->enMode));
                        v->writev("vGain", c->vGain, meta::noise_generator::NUM_GENERATORS);
                        v->write("fGainOut", c->fGainOut);
                        v->write("bActive", c->bActive);
                        v->write("vIn", c->vIn);
                        v->write("vOut", c->vOut);

                        // Audio Ports
                        v->write("pIn", c->pIn);
                        v->write("pOut", c->pOut);
                        v->write("pSlSw", c->pSlSw);
                        v->write("pMtSw", c->pMtSw);
                        v->write("pNoiseMode", c->pNoiseMode);
                        v->writev("pGain", c->pGain, meta::noise_generator::NUM_GENERATORS);
                        v->write("pGainOut", c->pGainOut);
                        v->write("pMeterIn", c->pMeterIn);
                        v->write("pMeterOut", c->pMeterOut);
                    }
                    v->end_object();
                }
            }
            v->end_array();

            // Write global data
            v->write("vBuffer", vBuffer);
            v->write("vTemp", vTemp);
            v->write("vFreqs", vFreqs);
            v->write("vFreqChart", vFreqChart);
            v->write("fGainIn", fGainIn);
            v->write("fGainOut", fGainOut);
            v->write("pData", pData);
            v->write_object("pIDisplay", pIDisplay);

            // Dump global ports
            v->write("pBypass", pBypass);
            v->write("pGainIn", pGainIn);
            v->write("pGainOut", pGainOut);
        }

    } /* namespace plugins */
} /* namespace lsp */

