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
            pData           = NULL;
            pIDisplay       = NULL;

            pBypass         = NULL;
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

        dspu::lcg_dist_t noise_generator::get_lcg_dist(size_t portValue)
        {
            switch (portValue)
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

        dspu::vn_velvet_type_t noise_generator::get_velvet_type(size_t portValue)
        {
            switch (portValue)
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

        dspu::ng_color_t noise_generator::get_color(size_t portValue)
        {
            switch (portValue)
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

        dspu::stlt_slope_unit_t noise_generator::get_color_slope_unit(size_t portValue)
        {
            switch (portValue)
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

        dspu::ng_generator_t noise_generator::get_generator_type(size_t portValue)
        {
            switch (portValue)
            {
                case meta::noise_generator_metadata::NOISE_TYPE_MLS:
                    return dspu::NG_GEN_MLS;
                case meta::noise_generator_metadata::NOISE_TYPE_VELVET:
                    return dspu::NG_GEN_VELVET;
                case meta::noise_generator_metadata::NOISE_TYPE_LCG:
                default:
                    return dspu::NG_GEN_LCG;
            }
        }

        noise_generator::ch_mode_t noise_generator::get_channel_mode(size_t portValue)
        {
            switch (portValue)
            {
                case meta::noise_generator_metadata::NOISE_MODE_ADD:
                    return CH_MODE_ADD;
                case meta::noise_generator_metadata::NOISE_MODE_MULT:
                    return CH_MODE_MULT;
                case meta::noise_generator_metadata::NOISE_MODE_OVERWRITE:
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
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t chr_sz           = meta::noise_generator::MESH_POINTS *  sizeof(float);
            size_t alloc            = szof_channels + 2*buf_sz + 3*chr_sz + nChannels * chr_sz;

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

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->vFreqChart       = reinterpret_cast<float *>(ptr);
                ptr                += chr_sz;
            }

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Construct in-place DSP processors
                c->sBypass.construct();
                c->sNoiseGenerator.construct();
                c->sAudibleStop.construct();

                // We seed every noise generator differently so that they produce uncorrelated noise.
                // We set the MLS number of bits to -1 so that the initialiser sets it to maximum.
                c->sNoiseGenerator.init(
                    -1, make_seed(),
                    make_seed(),
                    make_seed(), -1, make_seed());

                // We also set the inaudible noise filter main properties. These are not user configurable.
                c->sAudibleStop.set_order(INA_FILTER_ORD);
                c->sAudibleStop.set_filter_type(dspu::BW_FLT_TYPE_HIGHPASS);

                // Same with colour
                c->sNoiseGenerator.set_coloring_order(COLOR_FILTER_ORDER);

                // Initialise fields
                c->enMode 				= CH_MODE_OVERWRITE;

                c->pIn                  = NULL;
                c->pOut                 = NULL;

                c->pLCGdist             = NULL;
                c->pVelvetType 			= NULL;
                c->pVelvetWin			= NULL;
                c->pVelvetARNd			= NULL;
                c->pVelvetCSW			= NULL;
                c->pVelvetCpr			= NULL;
                c->pColorSel			= NULL;
                c->pCslopeNPN			= NULL;
                c->pCslopeDBO			= NULL;
                c->pCslopeDBD			= NULL;
                c->pNoiseType			= NULL;
                c->pNoiseMode			= NULL;
                c->pAmplitude			= NULL;
                c->pOffset				= NULL;
                c->pInaSw				= NULL;
                c->pMsh                 = NULL;
                c->pSlSw                = NULL;
                c->pMtSw                = NULL;

                c->bUpdPlots            = true;
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            lsp_trace("Binding audio ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                vChannels[i].pIn            = TRACE_PORT(ports[port_id++]);
                vChannels[i].pOut           = TRACE_PORT(ports[port_id++]);
            }

            // Bind global ports
            lsp_trace("Binding global control ports");
            pBypass                     = TRACE_PORT(ports[port_id++]);

            // Bind channel control ports
            lsp_trace("Binding channel control ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                vChannels[i].pLCGdist		= TRACE_PORT(ports[port_id++]);

                vChannels[i].pVelvetType 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pVelvetWin 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pVelvetARNd 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pVelvetCSW 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pVelvetCpr 	= TRACE_PORT(ports[port_id++]);

                vChannels[i].pColorSel 		= TRACE_PORT(ports[port_id++]);
                vChannels[i].pCslopeNPN 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pCslopeDBO 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pCslopeDBD 	= TRACE_PORT(ports[port_id++]);

                vChannels[i].pNoiseType 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pNoiseMode 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pAmplitude 	= TRACE_PORT(ports[port_id++]);
                vChannels[i].pOffset 		= TRACE_PORT(ports[port_id++]);
                vChannels[i].pInaSw 		= TRACE_PORT(ports[port_id++]);

                vChannels[i].pMsh           = TRACE_PORT(ports[port_id++]);
            }

            // Channel switches only exists on multi-channel versions. Skip for 1X plugin.
            if (nChannels > 1)
            {
                lsp_trace("Binding channel switches ports");
                for (size_t i=0; i<nChannels; ++i)
                {
                    vChannels[i].pSlSw 		= TRACE_PORT(ports[port_id++]);
                    vChannels[i].pMtSw 		= TRACE_PORT(ports[port_id++]);
                }
            }
        }

        void noise_generator::destroy()
        {
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
                    c->vFreqChart   = NULL;
                    c->sNoiseGenerator.destroy();
                }
                vChannels = NULL;
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

            // Update sample rate
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->sBypass.init(sr);
                c->bForceAudible = (0.5f * sr) < INA_FILTER_CUTOFF;
                c->sNoiseGenerator.set_sample_rate(sr);
                c->sAudibleStop.set_sample_rate(sr);
                c->sAudibleStop.set_cutoff_frequency(INA_FILTER_CUTOFF);
            }
        }

        void noise_generator::update_settings()
        {
            // Check if one of the channels is solo.
            bool has_solo   = false;
            bool bypass     = pBypass->value() >= 0.5f;
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                if ((c->pSlSw != NULL) && (c->pSlSw->value() >= 0.5f))
                {
                    has_solo = true;
                    break;
                }
            }

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Update bypass
                c->sBypass.set_bypass(bypass);

                // If one of the channels is solo, then we simply know from the solo switch if this channel
                // is active. Otherwise, we check whether the channel was set to mute or not.
                bool solo               = (c->pSlSw != NULL) ? c->pSlSw->value() >= 0.5f : false;
                bool mute               = (c->pMtSw != NULL) ? c->pMtSw->value() >= 0.5f : false;
                c->bActive              = (has_solo) ? solo : !mute;
                c->enMode               = get_channel_mode(c->pNoiseMode->value());
                c->bInaudible          = (c->bForceAudible) ? false : c->pInaSw->value() >= 0.5f;

                // Configure noise generator
                dspu::lcg_dist_t lcgdist = get_lcg_dist(c->pLCGdist->value());
                dspu::vn_velvet_type_t velvettype = get_velvet_type(c->pVelvetType->value());
                bool velvetcs           = c->pVelvetCSW->value() >= 0.5f;
                float velvetcsp         = c->pVelvetCpr->value() * 0.01f;
                dspu::ng_color_t color  = (c->bInaudible) ? dspu::NG_COLOR_WHITE : get_color(c->pColorSel->value());
                dspu::ng_generator_t noise_type = get_generator_type(c->pNoiseType->value());
                dspu::stlt_slope_unit_t color_slope_unit    = get_color_slope_unit(c->pColorSel->value());

                float color_slope       = -0.5f;
                switch (color_slope_unit)
                {
                    case dspu::STLT_SLOPE_UNIT_DB_PER_OCTAVE:
                        color_slope         = c->pCslopeDBO->value();
                        break;

                    case dspu::STLT_SLOPE_UNIT_DB_PER_DECADE:
                        color_slope         = c->pCslopeDBD->value();
                        break;

                    case dspu::STLT_SLOPE_UNIT_NEPER_PER_NEPER:
                    default:
                        color_slope         = c->pCslopeNPN->value();
                        break;
                }

                // If the noise has to be inaudible we are best setting it to white, or excessive high frequency boost will make it audible.
                // Conversely, excessive low frequency attenuation will make it non-existent.
                c->sNoiseGenerator.set_lcg_distribution(lcgdist);
                c->sNoiseGenerator.set_velvet_type(velvettype);
                c->sNoiseGenerator.set_velvet_window_width(c->pVelvetWin->value());
                c->sNoiseGenerator.set_velvet_arn_delta(c->pVelvetARNd->value());
                c->sNoiseGenerator.set_velvet_crush(velvetcs);
                c->sNoiseGenerator.set_velvet_crushing_probability(velvetcsp);
                c->sNoiseGenerator.set_noise_color(color);
                c->sNoiseGenerator.set_color_slope(color_slope, color_slope_unit);
                c->sNoiseGenerator.set_generator(noise_type);
                c->sNoiseGenerator.set_amplitude(c->pAmplitude->value());
                c->sNoiseGenerator.set_offset(c->pOffset->value());

                // Plots only really need update when we operate the controls, se we set the update to true
                c->bUpdPlots = true;
        	}

            // Query inline display redraw
            pWrapper->query_display_draw();
        }

        void noise_generator::process(size_t samples)
        {
            // Process each channel independently
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

                // Get input and output buffers
                const float *in         = c->pIn->buffer<float>();
                float *out              = c->pOut->buffer<float>();
                if ((in == NULL) || (out == NULL))
                    continue;

                // Process the noise generator
                for (size_t count = samples; count > 0;)
                {
                    size_t to_do    = lsp_min(count, BUFFER_SIZE);
                    if (c->bActive)
                    {
                        if (c->bInaudible)
                        {
                            c->sNoiseGenerator.process_overwrite(vTemp, to_do);
                            dsp::mul_k2(vTemp, INA_ATTENUATION, to_do);

                            switch (c->enMode)
                            {
                                case CH_MODE_ADD:
                                    dsp::copy(vBuffer, in, to_do);
                                    c->sAudibleStop.process_add(vBuffer, vTemp, to_do);
                                    break;

                                case CH_MODE_MULT:
                                    dsp::copy(vBuffer, in, to_do);
                                    c->sAudibleStop.process_mul(vBuffer, vTemp, to_do);
                                    break;

                                case CH_MODE_OVERWRITE:
                                default:
                                    c->sAudibleStop.process_overwrite(vBuffer, vTemp, to_do);
                                    break;
                            }
                        }
                        else // !bInaudible
                        {
                            switch (c->enMode)
                            {
                                case CH_MODE_ADD:
                                    c->sNoiseGenerator.process_add(vBuffer, in, count);
                                    break;

                                case CH_MODE_MULT:
                                    c->sNoiseGenerator.process_mul(vBuffer, in, count);
                                    break;

                                case CH_MODE_OVERWRITE:
                                default:
                                    c->sNoiseGenerator.process_overwrite(vBuffer, count);
                                    break;
                            }
                        }
                    }
                    else
                        dsp::copy(vBuffer, in, count);

                    // Post-process buffer
                    c->sBypass.process(out, in, vBuffer, to_do);

                    // Update pointers and proceed
                    in      += to_do;
                    out     += to_do;
                    count   -= to_do;
                }

                // Make a Frequency Chart - It only needs to be updated when the settings changed. so if bUpdPlots is true.
                // We do the chart after processing so that we chart the most up to date filter state.
                if (!c->bUpdPlots)
                    continue;

                plug::mesh_t *mesh = c->pMsh->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    // Compute frequency characteristics
                    if (c->bActive)
                    {
                        c->sNoiseGenerator.freq_chart(vFreqChart, vFreqs, meta::noise_generator_metadata::MESH_POINTS);
                        dsp::pcomplex_mod(c->vFreqChart, vFreqChart, meta::noise_generator_metadata::MESH_POINTS);
                    }
                    else
                        dsp::fill_zero(c->vFreqChart, meta::noise_generator_metadata::MESH_POINTS);


                    // Commit frequency characteristics to output mesh
                    dsp::copy(&mesh->pvData[0][2], vFreqs, meta::noise_generator_metadata::MESH_POINTS);
                    dsp::copy(&mesh->pvData[1][2], c->vFreqChart, meta::noise_generator_metadata::MESH_POINTS);

                    // Add extra points
                    mesh->pvData[0][0] = SPEC_FREQ_MIN*0.5f;
                    mesh->pvData[0][1] = SPEC_FREQ_MIN*0.5f;
                    mesh->pvData[0][meta::noise_generator_metadata::MESH_POINTS+2] = SPEC_FREQ_MAX*2.0f;
                    mesh->pvData[0][meta::noise_generator_metadata::MESH_POINTS+3] = SPEC_FREQ_MAX*2.0f;

                    mesh->pvData[1][0] = GAIN_AMP_0_DB;
                    mesh->pvData[1][1] = c->vFreqChart[0];
                    mesh->pvData[1][meta::noise_generator_metadata::MESH_POINTS+2] = c->vFreqChart[meta::noise_generator_metadata::MESH_POINTS-1];
                    mesh->pvData[1][meta::noise_generator_metadata::MESH_POINTS+3] = GAIN_AMP_0_DB;

                    mesh->data(2, meta::noise_generator_metadata::MESH_POINTS + 4);

                    // Update state only
                    c->bUpdPlots = false;
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

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Perform amplitude decimation
                for (size_t j=0; j<width; ++j)
                {
                    size_t k        = (j*meta::noise_generator_metadata::MESH_POINTS)/width;
                    b->v[1][j+2]    = c->vFreqChart[k];
                }
                b->v[1][1]      = b->v[1][2];
                b->v[1][width+2]= b->v[1][width+1];

                dsp::fill(b->v[3], height, width + 4);
                dsp::axis_apply_log1(b->v[3], b->v[1], zy, dy, width+4);

                // Draw mesh
                col.hue(float(i) / float(nChannels));
                uint32_t color = (bypassing || !(active())) ? CV_SILVER : col.rgb24();
                Color stroke(color), fill(color, 0.5f);
                cv->draw_poly(b->v[2], b->v[3], width+4, stroke, fill);
            }

            return true;
        }

        void noise_generator::dump(dspu::IStateDumper *v) const
        {
            // It is very useful to dump plugin state for debug purposes
            v->write("nChannels", nChannels);
            v->begin_array("vChannels", vChannels, nChannels);

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                v->begin_object(c, sizeof(channel_t));
                {
                    v->write_object("sNoiseGenerator", &c->sNoiseGenerator);
                    v->write_object("sAudibleStop", &c->sAudibleStop);

                    v->write("enMode", size_t(c->enMode));
                    v->write("bActive", c->bActive);
                    v->write("bInaudible", c->bInaudible);
                    v->write("bForceAudible", c->bForceAudible);
                    v->write("bUpdPlots",c->bUpdPlots);

                    v->write("pIn", c->pIn);
                    v->write("pOut", c->pOut);

                    v->write("vFreqChart", c->vFreqChart);

                    v->write("pLCGdist", c->pLCGdist);
                    v->write("pVelvetType", c->pVelvetType);
                    v->write("pVelvetWin", c->pVelvetWin);
                    v->write("pVelvetARNd", c->pVelvetARNd);
                    v->write("pVelvetCSW", c->pVelvetCSW);
                    v->write("pVelvetCpr", c->pVelvetCpr);
                    v->write("pColorSel", c->pColorSel);
                    v->write("pCslopeNPN", c->pCslopeNPN);
                    v->write("pCslopeDBO", c->pCslopeDBO);
                    v->write("pCslopeDBD", c->pCslopeDBD);
                    v->write("pNoiseType", c->pNoiseType);
                    v->write("pNoiseMode", c->pNoiseMode);
                    v->write("pAmplitude", c->pAmplitude);
                    v->write("pOffset", c->pOffset);
                    v->write("pInaSw", c->pInaSw);
                    v->write("pMsh", c->pMsh);
                    v->write("pSlSw", c->pSlSw);
                    v->write("pMtSw", c->pMtSw);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vBuffer", vBuffer);
            v->write("vTemp", vTemp);
            v->write("vFreqs", vFreqs);
            v->write("vFreqChart", vFreqChart);
            v->write("pData", pData);
            v->write_object("pIDisplay", pIDisplay);
        }

    } /* namespace plugins */
} /* namespace lsp */

