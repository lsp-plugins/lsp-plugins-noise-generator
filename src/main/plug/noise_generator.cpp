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

#include <private/plugins/noise_generator.h>

/* The size of temporary buffer for audio processing */
#define BUFFER_SIZE         0x1000U
#define INA_FILTER_ORD 		32
#define INA_FILTER_CUTOFF 	20e3
#define COLOR_FILTER_ORDER  32

#define TRACE_PORT(p)       lsp_trace("  port id=%s", (p)->metadata()->id);

namespace lsp
{
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

            pChSel          = NULL;

            pLCGdist        = NULL;
            pVelvetType 	= NULL;
			pVelvetWin 		= NULL;
			pVelvetARNd		= NULL;
			pVelvetCSW		= NULL;
			pVelvetCpr		= NULL;
			pColorSel		= NULL;
			pCslopeNPN		= NULL;
			pCslopeDBO		= NULL;
			pCslopeDBD		= NULL;
			pNoiseType		= NULL;
			pNoiseMode		= NULL;
			pAmplitude		= NULL;
			pOffset			= NULL;
			pInaSw			= NULL;

            pData           = NULL;
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
				default:
				case meta::noise_generator_metadata::NOISE_LCG_GAUSSIAN:
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
                default:
                case meta::noise_generator_metadata::NOISE_VELVET_OVN:
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
        		default:
        		case meta::noise_generator_metadata::NOISE_COLOR_WHITE:
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
        		default:
        		case meta::noise_generator_metadata::NOISE_COLOR_ARBITRARY_NPN:
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
				default:
				case meta::noise_generator_metadata::NOISE_TYPE_LCG:
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
        		default:
        		case meta::noise_generator_metadata::NOISE_MODE_OVERWRITE:
        			return CH_MODE_OVERWRITE;
        	}
        }

        void noise_generator::init_state_stage(channel_t *c)
        {
        	c->sChUpd.nUpdate = 0;

        	c->sChUpd.sStateStage.nPV_pLCGdist = meta::noise_generator_metadata::NOISE_LCG_DFL;
        	c->sChUpd.nUpdate |= UPD_LCG_DIST;

        	c->sChUpd.sStateStage.nPV_pVelvetType = meta::noise_generator_metadata::NOISE_VELVET_DFL;
        	c->sChUpd.nUpdate |= UPD_VELVET_TYPE;
        	c->sChUpd.sStateStage.fPV_pVelvetWin = meta::noise_generator_metadata::VELVET_WINDOW_DURATION_DFL;
        	c->sChUpd.nUpdate |= UPD_VELVET_WIN;
        	c->sChUpd.sStateStage.fPV_pVelvetARNd = meta::noise_generator_metadata::VELVET_ARN_DELTA_DFL;
        	c->sChUpd.nUpdate |= UPD_VELVET_ARN_D;
        	c->sChUpd.sStateStage.bPV_pVelvetCSW = false;
        	c->sChUpd.nUpdate |= UPD_VELVET_CRUSH;
        	c->sChUpd.sStateStage.fPV_pVelvetCpr = meta::noise_generator_metadata::VELVET_CRUSH_PROB_DFL;
        	c->sChUpd.nUpdate |= UPD_VELVET_CRUSH_P;

        	c->sChUpd.sStateStage.nPV_pColorSel = meta::noise_generator_metadata::NOISE_COLOR_DFL;
        	c->sChUpd.nUpdate |= UPD_COLOR;
        	c->sChUpd.sStateStage.fPV_pCslopeNPN = meta::noise_generator_metadata::NOISE_COLOR_SLOPE_NPN_DFL;
        	c->sChUpd.sStateStage.fPV_pCslopeDBO = meta::noise_generator_metadata::NOISE_COLOR_SLOPE_DBO_DFL;
        	c->sChUpd.sStateStage.fPV_pCslopeDBD = meta::noise_generator_metadata::NOISE_COLOR_SLOPE_DBD_DFL;
        	c->sChUpd.nUpdate |= UPD_COLOR_SLOPE;

        	c->sChUpd.sStateStage.nPV_pNoiseType = meta::noise_generator_metadata::NOISE_TYPE_DFL;
        	c->sChUpd.nUpdate |= UPD_NOISE_TYPE;
        	c->sChUpd.sStateStage.nPV_pNoiseMode = meta::noise_generator_metadata::NOISE_MODE_DFL;
        	c->sChUpd.nUpdate |= UPD_NOISE_MODE;
        	c->sChUpd.sStateStage.fPV_pAmplitude = meta::noise_generator_metadata::NOISE_AMPLITUDE_DFL;
        	c->sChUpd.nUpdate |= UPD_NOISE_AMPLITUDE;
        	c->sChUpd.sStateStage.fPV_pOffset = meta::noise_generator_metadata::NOISE_OFFSET_DFL;
        	c->sChUpd.nUpdate |= UPD_NOISE_OFFSET;

        	c->sChUpd.bUseGlobal = false;
        	c->sChUpd.bActive = false;
        	c->sChUpd.bInaudible = false;
        }

        void noise_generator::commit_staged_state_change(channel_t *c)
        {
            if (c->sChUpd.nUpdate == 0)
                return;

            if (c->sChUpd.nUpdate & UPD_LCG_DIST)
            {
                c->enLCGDist = get_lcg_dist(c->sChUpd.sStateStage.nPV_pLCGdist);
                c->sNoiseGenerator.set_lcg_distribution(c->enLCGDist);
            }

            if (c->sChUpd.nUpdate & UPD_VELVET_TYPE)
            {
                c->enVelvetType = get_velvet_type(c->sChUpd.sStateStage.nPV_pVelvetType);
                c->sNoiseGenerator.set_velvet_type(c->enVelvetType);
            }

            if (c->sChUpd.nUpdate & UPD_VELVET_WIN)
            {
                // No conversion needed
                c->fVelvetWin = c->sChUpd.sStateStage.fPV_pVelvetWin;
                c->sNoiseGenerator.set_velvet_window_width(c->fVelvetWin);
            }

            if (c->sChUpd.nUpdate & UPD_VELVET_ARN_D)
            {
                // No conversion needed
                c->fVelvetARNd = c->sChUpd.sStateStage.fPV_pVelvetARNd;
                c->sNoiseGenerator.set_velvet_arn_delta(c->fVelvetARNd);
            }

            if (c->sChUpd.nUpdate & UPD_VELVET_CRUSH)
            {
                // No conversion needed
                c->bVelvetCrush = c->sChUpd.sStateStage.bPV_pVelvetCSW;
                c->sNoiseGenerator.set_velvet_crush(c->bVelvetCrush);
            }

            if (c->sChUpd.nUpdate & UPD_VELVET_CRUSH_P)
            {
                // Percent to 0-1
                c->fVelvetCrushP = c->sChUpd.sStateStage.fPV_pVelvetCpr / 100.0f;
                c->sNoiseGenerator.set_velvet_crushing_probability(c->fVelvetCrushP);
            }

            if (c->sChUpd.nUpdate & UPD_COLOR)
            {
                c->enColor = get_color(c->sChUpd.sStateStage.nPV_pColorSel);
                c->sNoiseGenerator.set_noise_color(c->enColor);
            }

            if ((c->sChUpd.nUpdate & UPD_COLOR_SLOPE) || (c->sChUpd.nUpdate & UPD_COLOR_SLOPE_UNIT))
            {
                c->enColorSlopeUnit = get_color_slope_unit(c->sChUpd.sStateStage.nPV_pColorSel);
                switch (c->enColorSlopeUnit)
                {

                    case dspu::STLT_SLOPE_UNIT_DB_PER_OCTAVE:
                    {
                        c->fColorSlope = c->sChUpd.sStateStage.fPV_pCslopeDBO;
                    }
                    break;

                    case dspu::STLT_SLOPE_UNIT_DB_PER_DECADE:
                    {
                        c->fColorSlope = c->sChUpd.sStateStage.fPV_pCslopeDBD;
                    }
                    break;

                    default:
                    case dspu::STLT_SLOPE_UNIT_NEPER_PER_NEPER:
                    {
                        c->fColorSlope = c->sChUpd.sStateStage.fPV_pCslopeNPN;
                    }
                    break;
                }

                c->sNoiseGenerator.set_color_slope(c->fColorSlope, c->enColorSlopeUnit);
            }

            if (c->sChUpd.nUpdate & UPD_NOISE_TYPE)
            {
                c->enNoiseType = get_generator_type(c->sChUpd.sStateStage.nPV_pNoiseType);
                c->sNoiseGenerator.set_generator(c->enNoiseType);
            }

            if (c->sChUpd.nUpdate & UPD_NOISE_MODE)
            {
                c->enMode = get_channel_mode(c->sChUpd.sStateStage.nPV_pNoiseMode);
            }

            if (c->sChUpd.nUpdate & UPD_NOISE_AMPLITUDE)
            {
                // Need conversion?
                c->fAmplitude = c->sChUpd.sStateStage.fPV_pAmplitude;
                c->sNoiseGenerator.set_amplitude(c->fAmplitude);
            }

            if (c->sChUpd.nUpdate & UPD_NOISE_OFFSET)
            {
                // No conversion needed
                c->fOffset = c->sChUpd.sStateStage.fPV_pOffset;
                c->sNoiseGenerator.set_offset(c->fOffset);
            }

            c->sNoiseGenerator.update_settings();

            c->sChUpd.nUpdate = 0;
        }

        void noise_generator::init(plug::IWrapper *wrapper, plug::IPort **ports)
        {
            // Call parent class for initialisation
            Module::init(wrapper, ports);

            // Estimate the number of bytes to allocate
            size_t szof_channels    = align_size(sizeof(channel_t) * nChannels, OPTIMAL_ALIGN);
            size_t buf_sz           = BUFFER_SIZE * sizeof(float);
            size_t alloc            = szof_channels + buf_sz;

            // Allocate memory-aligned data
            uint8_t *ptr            = alloc_aligned<uint8_t>(pData, alloc, OPTIMAL_ALIGN);
            if (ptr == NULL)
                return;

            // Initialise pointers to channels and temporary buffer
            vChannels               = reinterpret_cast<channel_t *>(ptr);
            ptr                    += szof_channels;
            vBuffer                 = reinterpret_cast<float *>(ptr);
            ptr                    += buf_sz;

            for (size_t i=0; i < nChannels; ++i)
            {
                channel_t *c            = &vChannels[i];

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
                c->sAudibleStop.set_cutoff_frequency(INA_FILTER_CUTOFF);
                c->sAudibleStop.set_filter_type(dspu::BW_FLT_TYPE_LOWPASS);

                // Same with colour
                c->sNoiseGenerator.set_coloring_order(COLOR_FILTER_ORDER);

                // Initialise fields
                c->enLCGDist            = dspu::LCG_UNIFORM;

                c->enVelvetType         = dspu::VN_VELVET_OVN;
                c->fVelvetWin           = 0.1f;
                c->fVelvetARNd 			= 0.5f;
                c->bVelvetCrush 		= false;
                c->fVelvetCrushP 		= 0.5f;

                c->enColor				= dspu::NG_COLOR_WHITE;
                c->fColorSlope 			= -0.5f;
                c->enColorSlopeUnit 	= dspu::STLT_SLOPE_UNIT_NEPER_PER_NEPER;

                c->enNoiseType 			= dspu::NG_GEN_LCG;
                c->enMode 				= CH_MODE_OVERWRITE;
                c->fAmplitude 			= 1.0f;
                c->fOffset 				= 0.0f;

                c->pIn                  = NULL;
                c->pOut                 = NULL;

                c->pGlSw                = NULL;
                c->pSlSw                = NULL;
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
            }

            // Bind ports
            lsp_trace("Binding ports");
            size_t port_id      = 0;

            // Bind input audio ports
            lsp_trace("Binding audio ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(ports[port_id]);
                vChannels[i].pIn    = ports[port_id++];

                TRACE_PORT(ports[port_id]);
                vChannels[i].pOut   = ports[port_id++];
            }

            // Channel selector only exists on multi-channel versions. Skip for 1X plugin.
            if (nChannels > 1)
            {
            	lsp_trace("Binding channel selector port");
                TRACE_PORT(ports[port_id]);
                pChSel 				= ports[port_id++];
            }

            // Global ports only exists on multi-channel versions. Skip for 1X plugin.
            if (nChannels > 1)
            {
            	lsp_trace("Binding global control ports");

                TRACE_PORT(ports[port_id]);
                pLCGdist 			= ports[port_id++];

                TRACE_PORT(ports[port_id]);
                pVelvetType 		= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pVelvetWin 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pVelvetARNd 		= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pVelvetCSW 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pVelvetCpr 			= ports[port_id++];

                TRACE_PORT(ports[port_id]);
                pColorSel 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pCslopeNPN 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pCslopeDBO 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pCslopeDBD 			= ports[port_id++];

                TRACE_PORT(ports[port_id]);
                pNoiseType 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pNoiseMode 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pAmplitude 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pOffset 			= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                pInaSw 				= ports[port_id++];
            }

            lsp_trace("Binding channel control ports");
            for (size_t i=0; i<nChannels; ++i)
            {
                TRACE_PORT(ports[port_id]);
                vChannels[i].pLCGdist		= ports[port_id++];

                TRACE_PORT(ports[port_id]);
                vChannels[i].pVelvetType 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pVelvetWin 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pVelvetARNd 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pVelvetCSW 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pVelvetCpr 	= ports[port_id++];

                TRACE_PORT(ports[port_id]);
                vChannels[i].pColorSel 		= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pCslopeNPN 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pCslopeDBO 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pCslopeDBD 	= ports[port_id++];

                TRACE_PORT(ports[port_id]);
                vChannels[i].pNoiseType 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pNoiseMode 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pAmplitude 	= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pOffset 		= ports[port_id++];
                TRACE_PORT(ports[port_id]);
                vChannels[i].pInaSw 		= ports[port_id++];
            }


            // Channel switches only exists on multi-channel versions. Skip for 1X plugin.
            if (nChannels > 1)
            {
				lsp_trace("Binding channel switches ports");
				for (size_t i=0; i<nChannels; ++i)
				{
					TRACE_PORT(ports[port_id]);
					vChannels[i].pGlSw		= ports[port_id++];
					TRACE_PORT(ports[port_id]);
					vChannels[i].pSlSw 		= ports[port_id++];
					TRACE_PORT(ports[port_id]);
					vChannels[i].pMtSw 		= ports[port_id++];
				}
            }
        }

        void noise_generator::destroy()
        {
            Module::destroy();

            // Destroy channels
            if (vChannels != NULL)
            {
                for (size_t i=0; i<nChannels; ++i)
                {
                    channel_t *c    = &vChannels[i];
                    c->sNoiseGenerator.destroy();
                }
                vChannels   = NULL;
            }

            vBuffer     = NULL;

            // Free previously allocated data chunk
            if (pData != NULL)
            {
                free_aligned(pData);
                pData       = NULL;
            }
        }

        void noise_generator::update_sample_rate(long sr)
        {
            // Update sample rate for the bypass processors
            for (size_t i=0; i<nChannels; ++i)
            {
                channel_t *c    = &vChannels[i];
                c->sAudibleStop.set_sample_rate(sr);
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
        		channel_t *c = &vChannels[i];

        		// Global controls only actually exist for mult-channel plugins. Do not use for 1X.
				if (nChannels > 1)
					c->sChUpd.bUseGlobal = c->pGlSw->value() >= 0.5f;

				// If one of the channels is solo, then we simply know from the solo switch if this channel
				// is active. Otherwise, we check whether the channel was set to mute or not.
                bool solo       	= (c->pSlSw != NULL) ? c->pSlSw->value() >= 0.5f : false;
                bool mute       	= (c->pMtSw != NULL) ? c->pMtSw->value() >= 0.5f : false;
                c->sChUpd.bActive	= (has_solo) ? solo : !mute;

                size_t lcgdist = (c->sChUpd.bUseGlobal) ? pLCGdist->value() : c->pLCGdist->value();
                if (lcgdist != c->sChUpd.sStateStage.nPV_pLCGdist)
                {
                	c->sChUpd.sStateStage.nPV_pLCGdist = lcgdist;
                	c->sChUpd.nUpdate |= UPD_LCG_DIST;
                }

                size_t velvettype = (c->sChUpd.bUseGlobal) ? pVelvetType->value() : c->pVelvetType->value();
                if (velvettype != c->sChUpd.sStateStage.nPV_pVelvetType)
                {
                	c->sChUpd.sStateStage.nPV_pVelvetType = velvettype;
                	c->sChUpd.nUpdate |= UPD_VELVET_TYPE;
                }

                float velvetwin = (c->sChUpd.bUseGlobal) ? pVelvetWin->value() : c->pVelvetWin->value();
                if (velvetwin != c->sChUpd.sStateStage.fPV_pVelvetWin)
                {
                	c->sChUpd.sStateStage.fPV_pVelvetWin = velvetwin;
                	c->sChUpd.nUpdate |= UPD_VELVET_WIN;
                }

                float velvetarnd = (c->sChUpd.bUseGlobal) ? pVelvetARNd->value() : c->pVelvetARNd->value();
                if (velvetarnd != c->sChUpd.sStateStage.fPV_pVelvetARNd)
                {
                	c->sChUpd.sStateStage.fPV_pVelvetARNd = velvetarnd;
                	c->sChUpd.nUpdate |= UPD_VELVET_ARN_D;
                }

                bool velvetcs = (c->sChUpd.bUseGlobal) ? pVelvetCSW->value() >= 0.5f : c->pVelvetCSW->value() >= 0.5f;
                if (velvetcs != c->sChUpd.sStateStage.bPV_pVelvetCSW)
                {
                	c->sChUpd.sStateStage.bPV_pVelvetCSW = velvetcs;
                	c->sChUpd.nUpdate |= UPD_VELVET_CRUSH;
                }

                float velvetcsp = (c->sChUpd.bUseGlobal) ? pVelvetCpr->value() : c->pVelvetCpr->value();
                if (velvetcsp != c->sChUpd.sStateStage.fPV_pVelvetCpr)
                {
                	c->sChUpd.sStateStage.fPV_pVelvetCpr = velvetcsp;
                	c->sChUpd.nUpdate |= UPD_VELVET_CRUSH_P;
                }

                size_t color = (c->sChUpd.bUseGlobal) ? pColorSel->value() : c->pColorSel->value();
                if (color != c->sChUpd.sStateStage.nPV_pColorSel)
                {
                	c->sChUpd.sStateStage.nPV_pColorSel = color;
                	c->sChUpd.nUpdate |= UPD_COLOR;
                }

                float colorslope;
                // NPN
                colorslope = (c->sChUpd.bUseGlobal) ? pCslopeNPN->value() : c->pCslopeNPN->value();
                if (colorslope != c->sChUpd.sStateStage.fPV_pCslopeNPN)
                {
                	c->sChUpd.sStateStage.fPV_pCslopeNPN = colorslope;
                	c->sChUpd.nUpdate |= UPD_COLOR_SLOPE;
                }
                // DBO
                colorslope = (c->sChUpd.bUseGlobal) ? pCslopeDBO->value() : c->pCslopeDBO->value();
                if (colorslope != c->sChUpd.sStateStage.fPV_pCslopeDBO)
                {
                	c->sChUpd.sStateStage.fPV_pCslopeDBO = colorslope;
                	c->sChUpd.nUpdate |= UPD_COLOR_SLOPE;
                }
                // DBD
                colorslope = (c->sChUpd.bUseGlobal) ? pCslopeDBD->value() : c->pCslopeDBD->value();
                if (colorslope != c->sChUpd.sStateStage.fPV_pCslopeDBD)
                {
                	c->sChUpd.sStateStage.fPV_pCslopeDBD = colorslope;
                	c->sChUpd.nUpdate |= UPD_COLOR_SLOPE;
                }

                size_t noisetype = (c->sChUpd.bUseGlobal) ? pNoiseType->value() : c->pNoiseType->value();
                if (noisetype != c->sChUpd.sStateStage.nPV_pNoiseType)
                {
                	c->sChUpd.sStateStage.nPV_pNoiseType = noisetype;
                	c->sChUpd.nUpdate |= UPD_NOISE_TYPE;
                }

                size_t noisemode = (c->sChUpd.bUseGlobal) ? pNoiseMode->value() : c->pNoiseMode->value();
                if (noisemode != c->sChUpd.sStateStage.nPV_pNoiseMode)
                {
                	c->sChUpd.sStateStage.nPV_pNoiseMode = noisemode;
                	c->sChUpd.nUpdate |= UPD_NOISE_MODE;
                }

                float amp = (c->sChUpd.bUseGlobal) ? pAmplitude->value() : c->pAmplitude->value();
                if (amp != c->sChUpd.sStateStage.fPV_pAmplitude)
                {
                	c->sChUpd.sStateStage.fPV_pAmplitude = amp;
                	c->sChUpd.nUpdate |= UPD_NOISE_AMPLITUDE;
                }

                float off = (c->sChUpd.bUseGlobal) ? pOffset->value() : c->pOffset->value();
                if (off != c->sChUpd.sStateStage.fPV_pOffset)
                {
                	c->sChUpd.sStateStage.fPV_pOffset = off;
                	c->sChUpd.nUpdate |= UPD_NOISE_OFFSET;
                }

                c->sChUpd.bInaudible = (c->sChUpd.bUseGlobal) ? pInaSw->value() >= 0.5f : c->pInaSw->value() >= 0.5f;
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

                if (!c->sChUpd.bActive)
                {
                    dsp::fill_zero(out, samples);
                    return;
                }

                if (c->sChUpd.bInaudible)
                {
                    while (samples > 0)
                    {
                        size_t to_do = (samples > BUFFER_SIZE) ? BUFFER_SIZE : samples;
                        c->sNoiseGenerator.process_overwrite(vBuffer, to_do);

                        switch (c->enMode)
                        {
                            case CH_MODE_OVERWRITE:
                            {
                                c->sAudibleStop.process_overwrite(out, vBuffer, to_do);
                            }
                            break;

                            case CH_MODE_ADD:
                            {
                                c->sAudibleStop.process_add(out, vBuffer, to_do);
                            }
                            break;

                            case CH_MODE_MULT:
                            {
                                c->sAudibleStop.process_mul(out, vBuffer, to_do);
                            }
                            break;
                        }

                        out     += to_do;
                        samples -= to_do;
                    }
                }
                else
                {
                    switch (c->enMode)
                    {
                        case CH_MODE_OVERWRITE:
                        {
                            c->sNoiseGenerator.process_overwrite(out, samples);
                        }
                        break;

                        case CH_MODE_ADD:
                        {
                            c->sNoiseGenerator.process_add(out, in, samples);
                        }
                        break;

                        case CH_MODE_MULT:
                        {
                            c->sNoiseGenerator.process_mul(out, in, samples);
                        }
                        break;
                    }
                }
            }
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

                    v->begin_object("sChUpd", &c->sChUpd, sizeof(c->sChUpd));
                    {
                        v->write("nUpdate", &c->sChUpd.nUpdate);

                        v->begin_object("sStateStage", &c->sChUpd.sStateStage, sizeof(c->sChUpd.sStateStage));
                        {
                            v->write("nPV_pLCGdist", &c->sChUpd.sStateStage.nPV_pLCGdist);

                            v->write("nPV_pVelvetType", &c->sChUpd.sStateStage.nPV_pVelvetType);
                            v->write("fPV_pVelvetWin", &c->sChUpd.sStateStage.fPV_pVelvetWin);
                            v->write("fPV_pVelvetARNd", &c->sChUpd.sStateStage.fPV_pVelvetARNd);
                            v->write("bPV_pVelvetCSW", &c->sChUpd.sStateStage.bPV_pVelvetCSW);
                            v->write("fPV_pVelvetCpr", &c->sChUpd.sStateStage.fPV_pVelvetCpr);

                            v->write("nPV_pColorSel", &c->sChUpd.sStateStage.nPV_pColorSel);
                            v->write("fPV_pCslopeNPN", &c->sChUpd.sStateStage.fPV_pCslopeNPN);
                            v->write("fPV_pCslopeDBO", &c->sChUpd.sStateStage.fPV_pCslopeDBO);
                            v->write("fPV_pCslopeDBD", &c->sChUpd.sStateStage.fPV_pCslopeDBD);

                            v->write("nPV_pNoiseType", &c->sChUpd.sStateStage.nPV_pNoiseType);
                            v->write("nPV_pNoiseMode", &c->sChUpd.sStateStage.nPV_pNoiseMode);
                            v->write("fPV_pAmplitude", &c->sChUpd.sStateStage.fPV_pAmplitude);
                            v->write("fPV_pOffset", &c->sChUpd.sStateStage.fPV_pOffset);
                        }

                        v->write("bUseGlobal", &c->sChUpd.bUseGlobal);
                        v->write("bActive", &c->sChUpd.bActive);
                        v->write("bInaudible", &c->sChUpd.bInaudible);
                    }

                    v->write("enLCGDist", &c->enLCGDist);

                    v->write("enVelvetType", &c->enVelvetType);
                    v->write("fVelvetWin", &c->fVelvetWin);
                    v->write("fVelvetARNd", &c->fVelvetARNd);
                    v->write("bVelvetCrush", &c->bVelvetCrush);
                    v->write("fVelvetCrushP", &c->fVelvetCrushP);

                    v->write("enColor", &c->enColor);
                    v->write("fColorSlope", &c->fColorSlope);
                    v->write("enColorSlopeUnit", &c->enColorSlopeUnit);

                    v->write("enNoiseType", &c->enNoiseType);
                    v->write("enMode", &c->enMode);
                    v->write("fAmplitude", &c->fAmplitude);
                    v->write("fOffset", &c->fOffset);

                    v->write("pIn", &c->pIn);
                    v->write("pOut", &c->pOut);

                    v->write("pLCGdist", &c->pLCGdist);
                    v->write("pVelvetType", &c->pVelvetType);
                    v->write("pVelvetWin", &c->pVelvetWin);
                    v->write("pVelvetARNd", &c->pVelvetARNd);
                    v->write("pVelvetCSW", &c->pVelvetCSW);
                    v->write("pVelvetCpr", &c->pVelvetCpr);
                    v->write("pColorSel", &c->pColorSel);
                    v->write("pCslopeNPN", &c->pCslopeNPN);
                    v->write("pCslopeDBO", &c->pCslopeDBO);
                    v->write("pCslopeDBD", &c->pCslopeDBD);
                    v->write("pNoiseType", &c->pNoiseType);
                    v->write("pNoiseMode", &c->pNoiseMode);
                    v->write("pAmplitude", &c->pAmplitude);
                    v->write("pOffset", &c->pOffset);
                    v->write("pInaSw", &c->pInaSw);
                    v->write("pGlSw", &c->pGlSw);
                    v->write("pSlSw", &c->pSlSw);
                    v->write("pMtSw", &c->pMtSw);
                }
                v->end_object();
            }
            v->end_array();

            v->write("vBuffer", vBuffer);

            v->write("pChSel", pChSel);

            v->write("pLCGdist", pLCGdist);
            v->write("pVelvetType", pVelvetType);
            v->write("pVelvetWin", pVelvetWin);
            v->write("pVelvetARNd", pVelvetARNd);
            v->write("pVelvetCSW", pVelvetCSW);
            v->write("pVelvetCpr", pVelvetCpr);
            v->write("pColorSel", pColorSel);
            v->write("pCslopeNPN", pCslopeNPN);
            v->write("pCslopeDBO", pCslopeDBO);
            v->write("pCslopeDBD", pCslopeDBD);
            v->write("pNoiseType", pNoiseType);
            v->write("pNoiseMode", pNoiseMode);
            v->write("pAmplitude", pAmplitude);
            v->write("pOffset", pOffset);
            v->write("pInaSw", pInaSw);

            v->write("pData", pData);
        }

    }
}
