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

#include <private/plugins/noise_generator.h>

/* The size of temporary buffer for audio processing */
#define BUFFER_SIZE                 0x1000U
#define INA_FILTER_ORD 		        64
#define INA_FILTER_CUTOFF	        22050.0f
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
            vFreqs          = NULL;
            vChrtRe         = NULL;
            vChrtIm         = NULL;
            nFreqs          = 0;
            pData           = NULL;
            pIDisplay       = NULL;
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
             * 1X Temporary Buffer for Processing (BUFFER_SIZE)
             * 1X Frequency List (MESH_POINTS)
             * 1X Real Part of Frequency Response (MESH_POINTS)
             * 1X Imaginary Part of Frequency Response (MESH_POINTS)
             * nChannelsX Data for Inline Display (X) (MESH_POINTS)
             * nChannelsX Data for Inline Display (Y) (MESH_POINTS)
             */
            nFreqs                  = meta::noise_generator::MESH_POINTS;
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t chr_sz           = nFreqs *  sizeof(float);
            size_t alloc            = szof_channels + buf_sz + 3u * chr_sz + 2u * nChannels * chr_sz;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialise pointers to channels and temporary buffer
            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;
            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;
            vFreqs                  = reinterpret_cast<float *>(ptr);
            ptr                    += chr_sz;
            vChrtRe                 = reinterpret_cast<float *>(ptr);
            ptr                    += chr_sz;
            vChrtIm                 = reinterpret_cast<float *>(ptr);
            ptr                    += chr_sz;

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->vIDisplay_x      = reinterpret_cast<float *>(ptr);
                ptr                += chr_sz;
                c->vIDisplay_y      = reinterpret_cast<float *>(ptr);
                ptr                += chr_sz;
            }

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                // Construct in-place DSP processors
                c->sNoiseGenerator.construct();
                c->sAudibleStop.construct();

                // We seed every noise generator differently so that they produce uncorrelated noise.
                // We set the MLS number of bits to -1 so that the initialiser sets it to maximum.
                c->sNoiseGenerator.init(
                        -1, make_seed(),
                        make_seed(),
                        make_seed(), -1, make_seed()
                        );

                // We also set the inaudible noise filter main properties. These are not user configurable.
                c->sAudibleStop.set_order(INA_FILTER_ORD);
                c->sAudibleStop.set_filter_type(dspu::BW_FLT_TYPE_HIGHPASS);

                // Same with colour
                c->sNoiseGenerator.set_coloring_order(COLOR_FILTER_ORDER);

                // Initialise fields
                c->enMode 				= CH_MODE_OVERWRITE;

                c->pIn                  = NULL;
                c->pOut                 = NULL;

                c->nIDisplay            = 0;

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
            Module::destroy();

            vBuffer     = NULL;
            vFreqs      = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }

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
                    c->vIDisplay_x = NULL;
                    c->vIDisplay_y = NULL;
                    c->sNoiseGenerator.destroy();
                }
                vChannels = NULL;
            }

        }

        void noise_generator::update_sample_rate(long sr)
        {
            // We can fill vFreqs: regular grid from 0 to half sample rate (excluded).
            size_t n_bins = nFreqs * 2u - 1u; // Number of bins of an FFT that would yield n_freqs points
            for (size_t k = 0; k < nFreqs; ++k)
                vFreqs[k] = sr * float(k) / n_bins;

            // Update sample rate
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c = &vChannels[i];

                c->bForceAudible = (0.5f * sr < INA_FILTER_CUTOFF);
                c->sNoiseGenerator.set_sample_rate(sr);
                c->sAudibleStop.set_sample_rate(sr);
                c->sAudibleStop.set_cutoff_frequency(INA_FILTER_CUTOFF);
            }
        }

        void noise_generator::update_settings()
        {
            // Check if one of the channels is solo.
            bool has_solo = false;
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                bool solo       = (c->pSlSw != NULL) ? c->pSlSw->value() >= 0.5f : false;
                if (solo)
                {
                    has_solo = true;
                    break;
                }
            }

            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

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

                if (c->bActive)
                {
                    // We process the samples for this channel, so we modify a samples copy otherwise the next channel will process 0 samples.
                    size_t count = samples;

                    if (c->bInaudible)
                    {
                        while (count > 0)
                        {
                            size_t to_do    = lsp_min(count, BUFFER_SIZE);
                            c->sNoiseGenerator.process_overwrite(vBuffer, to_do);
                            dsp::mul_k2(vBuffer, INA_ATTENUATION, to_do);

                            switch (c->enMode)
                            {
                                case CH_MODE_ADD:
                                    dsp::copy(out, in, to_do);
                                    c->sAudibleStop.process_add(out, vBuffer, to_do);
                                    break;

                                case CH_MODE_MULT:
                                    dsp::copy(out, in, to_do);
                                    c->sAudibleStop.process_mul(out, vBuffer, to_do);
                                    break;

                                case CH_MODE_OVERWRITE:
                                default:
                                    c->sAudibleStop.process_overwrite(out, vBuffer, to_do);
                                    break;
                            }

                            in      += to_do;
                            out     += to_do;
                            count   -= to_do;
                        }
                    }
                    else // !c->bInaudible
                    {
                        switch (c->enMode)
                        {
                            case CH_MODE_ADD:
                                c->sNoiseGenerator.process_add(out, in, count);
                                break;

                            case CH_MODE_MULT:
                                c->sNoiseGenerator.process_mul(out, in, count);
                                break;

                            case CH_MODE_OVERWRITE:
                            default:
                                c->sNoiseGenerator.process_overwrite(out, count);
                                break;
                        }
                    }
                }
                else // !c->bActive
                {
                    dsp::fill_zero(out, samples);
                }

                // Make a Frequency Chart - It only needs to be updated when the settings changed. so if bUpdPlots is true.
                // We do the chart after processing so that we chart the most up to date filter state.
                if (!c->bUpdPlots)
                    continue;

                plug::mesh_t *mesh = c->pMsh->buffer<plug::mesh_t>();
                if ((mesh != NULL) && (mesh->isEmpty()))
                {
                    if (c->bActive)
                    {
                        dsp::copy(mesh->pvData[0], vFreqs, nFreqs);
                        c->sNoiseGenerator.freq_chart(vChrtRe, vChrtIm, vFreqs, nFreqs);
                        dsp::complex_mod(mesh->pvData[1], vChrtRe, vChrtIm, nFreqs);
                        mesh->data(2, nFreqs);

                        // Filling Inline Display Buffers by decimating main plot buffers
                        c->nIDisplay = lsp_min(IDISPLAY_BUF_SIZE, nFreqs);
                        size_t step = nFreqs / c->nIDisplay;
                        size_t head = 0;
                        for (size_t i = 0; i < c->nIDisplay; ++i)
                        {
                            c->vIDisplay_x[i] = mesh->pvData[0][head];
                            c->vIDisplay_y[i] = mesh->pvData[1][head];
                            head += step;
                        }

                    }
                    else
                        mesh->data(2, 0);
                }

                c->bUpdPlots = false;
            }
        }

        static const uint32_t ch_colors[] =
        {
            // x1
            0x0a9bff,
            // x2
            0xff0e11,
            0x0a9bff,
            // x4
            0xff0e11,
            0x12ff16,
            0xff6c11,
            0x0a9bff
        };

        bool noise_generator::inline_display(plug::ICanvas *cv, size_t width, size_t height)
        {
            // Check proportions
            if (height > width)
                height  = width;

            // Init canvas
            if (!cv->init(width, height))
                return false;
            width   = cv->width();
            height  = cv->height();
            float cx    = width >> 1;
            float cy    = height >> 1;

            // Clear background
            cv->paint();

            // Draw axis
            cv->set_line_width(1.0);
            cv->set_color_rgb(CV_SILVER, 0.5f);
            cv->line(0, 0, width, height);
            cv->line(0, height, width, 0);

            cv->set_color_rgb(CV_WHITE, 0.5f);
            cv->line(cx, 0, cx, height);
            cv->line(0, cy, width, cy);

            // Check for solos:
            const uint32_t *cols =
                    (nChannels < 2) ? &ch_colors[0] :
                    (nChannels < 4) ? &ch_colors[1] :
                    &ch_colors[3];

//            float halfv = 0.5f * width;
//            float halfh = 0.5f * height;

            // Estimate the display length
            size_t di_length = 1;
            for (size_t ch = 0; ch < nChannels; ++ch)
                di_length = lsp_max(di_length, vChannels[ch].nIDisplay);

            // Allocate buffer: t, f(t)
            pIDisplay = core::IDBuffer::reuse(pIDisplay, 2, di_length);
            core::IDBuffer *b = pIDisplay;
            if (b == NULL)
                return false;

            bool aa = cv->set_anti_aliasing(true);

            for (size_t ch = 0; ch < nChannels; ++ch)
            {
                channel_t *c = &vChannels[ch];
                if (!c->bActive)
                    continue;

                // We scale the contents so that they fill the width and height span:
                // X: Min to Max => 0 to Width
                // Y: Min to Max => 0 to Height
                float max_x = dsp::max(c->vIDisplay_x, c->nIDisplay);
                float min_x = dsp::min(c->vIDisplay_x, c->nIDisplay);
                float range_x = max_x - min_x;
                float max_y = dsp::max(c->vIDisplay_y, c->nIDisplay);
                float min_y = dsp::min(c->vIDisplay_y, c->nIDisplay);
                float range_y = max_y - min_y;
                size_t dlen = lsp_min(c->nIDisplay, di_length);
                for (size_t i=0; i<dlen; ++i)
                {
                    b->v[0][i] = width * c->vIDisplay_x[i] / range_x - width * min_x / range_x;
                    b->v[1][i] = height * c->vIDisplay_y[i] / range_y - height * min_y / range_y;
                }

                // Set color and draw
                cv->set_color_rgb(cols[ch]);
                cv->set_line_width(2);
                cv->draw_lines(b->v[0], b->v[1], dlen);
            }

            cv->set_anti_aliasing(aa);

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
            v->write("vFreqs", vFreqs);
            v->write("vChrtRe", vChrtRe);
            v->write("vChrtIm", vChrtIm);
            v->write("nFreqs", nFreqs);
            v->write("pData", pData);
            v->write_object("pIDisplay", pIDisplay);
        }

    }
}
