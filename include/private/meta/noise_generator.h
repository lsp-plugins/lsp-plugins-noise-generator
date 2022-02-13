/*
 * Copyright (C) 2020 Linux Studio Plugins Project <https://lsp-plug.in/>
 *           (C) 2020 Vladimir Sadovnikov <sadko4u@gmail.com>
 *
 * This file is part of lsp-plugins-noise-generator
 * Created on: 25 нояб. 2020 г.
 *
 * lsp-plugins-noise-generator is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * lsp-plugins-noise-generator is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with lsp-plugins-noise-generator. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PRIVATE_META_NOISE_GENERATOR_H_
#define PRIVATE_META_NOISE_GENERATOR_H_

#include <lsp-plug.in/plug-fw/meta/types.h>
#include <lsp-plug.in/plug-fw/const.h>

namespace lsp
{
    //-------------------------------------------------------------------------
    // Plugin metadata
    namespace meta
    {
        typedef struct noise_generator
        {
            static const float  SAMPLES_MIN         = 0.0f;
            static const float  SAMPLES_MAX         = 10000.0f;
            static const float  SAMPLES_DFL         = 0.0f;
            static const float  SAMPLES_STEP        = 1.0f;

            static const float  TIME_MIN            = 0.0f;
            static const float  TIME_MAX            = 1000.0f;
            static const float  TIME_DFL            = 0.0f;
            static const float  TIME_STEP           = 0.01f;

            static const float  DELAY_OUT_MAX_TIME  = 10.0f / MAX_SAMPLE_RATE;
        } noise_generator;

        // Plugin type metadata
        extern const plugin_t noise_generator_mono;
        extern const plugin_t noise_generator_stereo;
    }
}

#endif /* PRIVATE_META_NOISE_GENERATOR_H_ */
