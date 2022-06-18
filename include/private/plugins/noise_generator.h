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

#ifndef PRIVATE_PLUGINS_NOISE_GENERATOR_H_
#define PRIVATE_PLUGINS_NOISE_GENERATOR_H_

#include <lsp-plug.in/dsp-units/noise/Generator.h>
#include <lsp-plug.in/dsp-units/filters/ButterworthFilter.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <private/meta/noise_generator.h>

namespace lsp
{
    namespace plugins
    {
        /**
         * Base class for the latency compensation delay
         */
        class noise_generator: public plug::Module
        {
            private:
                noise_generator & operator = (const noise_generator &);
                noise_generator (const noise_generator &);

            protected:
                enum ch_update_t
                {
					UPD_NOISE_TYPE 			= 1 << 9,
					UPD_NOISE_MODE 			= 1 << 10,
					UPD_NOISE_AMPLITUDE 	= 1 << 11,
					UPD_NOISE_OFFSET 		= 1 << 12
                };

                enum ch_mode_t
				{
                	CH_MODE_OVERWRITE,
					CH_MODE_ADD,
					CH_MODE_MULT
				};

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::NoiseGenerator	sNoiseGenerator;	// Noise Generator
                    dspu::ButterworthFilter	sAudibleStop; 		// Filter to stop the audible band

                    // Parameters
                    ch_mode_t               enMode;             // The Channel Mode
                    bool                    bActive;
                    bool                    bInaudible;
                    bool                    bForceAudible;      // Use if the sample rate does not allow actual inaudible noise

                    // Audio Ports
                    plug::IPort        		*pIn;         		// Input port
                    plug::IPort        		*pOut;              // Output port

                    // Input ports
                    plug::IPort 			*pLCGdist; 			// LCG Distribution
                    plug::IPort				*pVelvetType; 		// Velvet Type
                    plug::IPort 			*pVelvetWin; 		// Velvet Window
                    plug::IPort 			*pVelvetARNd; 		// Velvet ARN Delta
                    plug::IPort 			*pVelvetCSW; 		// Velvet Crushing Switch
                    plug::IPort 			*pVelvetCpr; 		// Velvet Crushing Probability
                    plug::IPort 			*pColorSel; 		// Colour Selector
                    plug::IPort 			*pCslopeNPN; 		// Colour Slope [Neper-per-Neper]
                    plug::IPort 			*pCslopeDBO; 		// Colour Slope [dB-per-Octave]
                    plug::IPort 			*pCslopeDBD; 		// Colour Slope [dB-per-Decade]
                    plug::IPort 			*pNoiseType; 		// Noise Type Selector
                    plug::IPort 			*pNoiseMode; 		// Noise Mode Selector
                    plug::IPort 			*pAmplitude; 		// Noise Amplitude
                    plug::IPort 			*pOffset; 			// Noise Offset
                    plug::IPort 			*pInaSw; 			// Make-Inaudible-Switch
                    plug::IPort        		*pSlSw;        		// Solo Switch
                    plug::IPort        		*pMtSw;        		// Mute Switch

                } channel_t;

            protected:
                size_t         			nChannels;          // Number of channels
                channel_t          		*vChannels;          // Delay channels
                float              		*vBuffer;            // Temporary buffer for audio processing
                uint8_t            		*pData;              // Allocated data

            public:
                explicit noise_generator(const meta::plugin_t *meta);
                virtual ~noise_generator();

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports);
                void                destroy();

            public:
                virtual void        update_sample_rate(long sr);
                virtual void        update_settings();
                virtual void        process(size_t samples);
                virtual void        dump(dspu::IStateDumper *v) const;

            protected:
                inline ssize_t 					make_seed() const;
                static dspu::lcg_dist_t 		get_lcg_dist(size_t portValue);
                static dspu::vn_velvet_type_t   get_velvet_type(size_t portValue);
                static dspu::ng_color_t  		get_color(size_t portValue);
                static dspu::stlt_slope_unit_t 	get_color_slope_unit(size_t portValue);
                static dspu::ng_generator_t 	get_generator_type(size_t portValue);
                static ch_mode_t 				get_channel_mode(size_t portValue);
        };
    }
}


#endif /* PRIVATE_PLUGINS_NOISE_GENERATOR_H_ */
