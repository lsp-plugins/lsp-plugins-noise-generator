/*
 * Copyright (C) 2023 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2023 Stefano Tronci <stefano.tronci@protonmail.com>
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

#include <lsp-plug.in/dsp-units/ctl/Bypass.h>
#include <lsp-plug.in/dsp-units/filters/ButterworthFilter.h>
#include <lsp-plug.in/dsp-units/noise/Generator.h>
#include <lsp-plug.in/dsp-units/util/Analyzer.h>
#include <lsp-plug.in/plug-fw/plug.h>
#include <lsp-plug.in/plug-fw/core/IDBuffer.h>

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
                    UPD_NOISE_TYPE              = 1 << 9,
                    UPD_NOISE_MODE              = 1 << 10,
                    UPD_NOISE_AMPLITUDE         = 1 << 11,
                    UPD_NOISE_OFFSET            = 1 << 12
                };

                enum ch_mode_t
                {
                    CH_MODE_OVERWRITE,
                    CH_MODE_ADD,
                    CH_MODE_MULT
                };

                typedef struct generator_t
                {
                    dspu::NoiseGenerator    sNoiseGenerator;    // Noise Generator
                    dspu::ButterworthFilter sAudibleStop;       // Filter to stop the audible band

                    // Parameters
                    float                   fGain;              // The outpug gain of generator
                    bool                    bActive;
                    bool                    bInaudible;
                    bool                    bUpdPlots;          // Whehter to update the plots

                    // Buffers
                    float                  *vBuffer;            // Temporary buffer for generated data
                    float                  *vFreqChart;         // Frequency chart

                    // Input ports
                    plug::IPort            *pNoiseType;         // Noise Type Selector
                    plug::IPort            *pAmplitude;         // Noise Amplitude
                    plug::IPort            *pOffset;            // Noise Offset
                    plug::IPort            *pSlSw;              // Solo Switch
                    plug::IPort            *pMtSw;              // Mute Switch
                    plug::IPort            *pInaSw;             // Make-Inaudible-Switch
                    plug::IPort            *pLCGdist;           // LCG Distribution
                    plug::IPort            *pVelvetType;        // Velvet Type
                    plug::IPort            *pVelvetWin;         // Velvet Window
                    plug::IPort            *pVelvetARNd;        // Velvet ARN Delta
                    plug::IPort            *pVelvetCSW;         // Velvet Crushing Switch
                    plug::IPort            *pVelvetCpr;         // Velvet Crushing Probability
                    plug::IPort            *pColorSel;          // Colour Selector
                    plug::IPort            *pCslopeNPN;         // Colour Slope [Neper-per-Neper]
                    plug::IPort            *pCslopeDBO;         // Colour Slope [dB-per-Octave]
                    plug::IPort            *pCslopeDBD;         // Colour Slope [dB-per-Decade]
                    plug::IPort            *pFft;               // FFT analysis flag
                    plug::IPort            *pMeterOut;          // Output level meter
                    plug::IPort            *pMsh;               // Mesh for Filter Frequency Chart Plot
                    plug::IPort            *pSpectrum;          // Spectrum graph
                } generator_t;

                typedef struct channel_t
                {
                    // DSP processing modules
                    dspu::Bypass            sBypass;            // Bypass

                    // Parameters
                    ch_mode_t               enMode;             // The Channel Mode
                    float                   vGain[meta::noise_generator::NUM_GENERATORS];   // Gain for each generator
                    float                   fGainIn;            // Input gain
                    float                   fGainOut;           // Output gain
                    bool                    bActive;            // Activity flag
                    float                  *vInBuffer;          // Temporary buffer for processing data
                    float                  *vOutBuffer;         // Temporary buffer for processing data
                    float                  *vIn;                // Input buffer pointer
                    float                  *vOut;               // Output buffer pointer

                    // Audio Ports
                    plug::IPort            *pIn;                // Input port
                    plug::IPort            *pOut;               // Output port
                    plug::IPort            *pSlSw;              // Solo Switch
                    plug::IPort            *pMtSw;              // Mute Switch
                    plug::IPort            *pFftIn;             // Input FFT analysis flag
                    plug::IPort            *pFftOut;            // Output FFT analysis flag
                    plug::IPort            *pNoiseMode;         // Output Mode Selector
                    plug::IPort            *pGain[meta::noise_generator::NUM_GENERATORS];   // Generator input matrix
                    plug::IPort            *pGainIn;            // Input gain
                    plug::IPort            *pGainOut;           // Output gain
                    plug::IPort            *pMeterIn;           // Input level meter
                    plug::IPort            *pMeterOut;          // Output level meter
                    plug::IPort            *pSpectrumIn;        // Input spectrum analysis
                    plug::IPort            *pSpectrumOut;       // Output spectrum analysis
                } channel_t;

            protected:
                generator_t                 vGenerators[meta::noise_generator::NUM_GENERATORS];
                dspu::Analyzer              sAnalyzer;          // Spectrum analyzer
                size_t                      nChannels;          // Number of channels
                channel_t                  *vChannels;          // Noise Generator channels
                float                      *vFreqs;             // Frequency list
                uint32_t                   *vIndexes;           // Frequency indexes
                float                      *vFreqChart;         // Temporary buffer for frequency chart
                float                       fGainIn;            // Overall input gain
                float                       fGainOut;           // Overall output gain
                uint8_t                    *pData;              // Allocated data
                core::IDBuffer             *pIDisplay;          // Inline display buffer

                plug::IPort                *pBypass;            // Bypass
                plug::IPort                *pGainIn;            // Input gain
                plug::IPort                *pGainOut;           // Output gain
                plug::IPort                *pFftIn;             // Input FFT analysis
                plug::IPort                *pFftOut;            // Output FFT analysis
                plug::IPort                *pFftGen;            // FFT generator analysis
                plug::IPort                *pReactivity;        // FFT reactivity
                plug::IPort                *pShiftGain;         // FFT gain shift

            protected:
                void                do_destroy();

            protected:
                inline ssize_t                      make_seed() const;
                static dspu::lcg_dist_t             get_lcg_dist(size_t value);
                static dspu::vn_velvet_type_t       get_velvet_type(size_t value);
                static dspu::ng_color_t             get_color(size_t value);
                static dspu::stlt_slope_unit_t      get_color_slope_unit(size_t value);
                static ch_mode_t                    get_channel_mode(size_t value);

            public:
                explicit noise_generator(const meta::plugin_t *meta);
                virtual ~noise_generator() override;

                virtual void        init(plug::IWrapper *wrapper, plug::IPort **ports) override;
                void                destroy() override;

            public:
                virtual void        update_sample_rate(long sr) override;
                virtual void        update_settings() override;
                virtual void        process(size_t samples) override;
                virtual bool        inline_display(plug::ICanvas *cv, size_t width, size_t height) override;
                virtual void        dump(dspu::IStateDumper *v) const override;
        };

    } /* namespace plugins */
} /* namespace lsp */


#endif /* PRIVATE_PLUGINS_NOISE_GENERATOR_H_ */
