<?php
plugin_header();

$x1     =   strpos($PAGE, '_x1') > 0;
$x2     =   strpos($PAGE, '_x2') > 0;
$x4     =   strpos($PAGE, '_x4') > 0;
?>

<p>
	This plugin implements a flexible noise generator.
</p>

<p>
<? if ($x1) { ?>
	The noise generator channel has the following features:
<? } else { ?>
	Each noise generator channel has the following features:
<? } ?>
</p>
<ul>
	<li>An input channel.</li>
	<li>An output channel.</li>
	<li>Multiple types of noise generators.</li>
	<li>Arbirrary slope coloring filter.</li>
</ul>

<p><b>Controls:</b></p>

<p><b>'SPECTRUM GRAPH' section:</b></p>
<p>
	This section allows to visualise the spectrum of the input signal, output signal or signal generator
	output. This graph also allows to visualise the slope of the coloring filter.
</p>
<ul>
	<li><b>SPECTRUM GRAPH</b> - All FFT and filter response traces are plot on this graph. </li>
	<li>
		<b>FFT IN</b> - This control allows to plot the FFT of the input signal. Use the channel buttons
		to the left of this control to select which channels to plot.
	</li>
		<li>
		<b>FFT OUT</b> - This control allows to plot the FFT of the output signal. Use the channel buttons
		to the left of this control to select which channels to plot.
	</li>
	<li>
		<b>FFT GEN</b> - This control allows to plot the FFT of noise generator output. Use the channel buttons
		to the left of this control to select which channels to plot.
	</li>
</ul>

<p><b>'Signal' section:</b></p>
<p>
 	This section allows to set the levels for each channel. The following settings are available for each
 	channel.
</p>
<ul>
	<li>
		<b>MODE</b> - This control sets how the input signal and noise generator output are combined to
		yield the output signal. Available modes are below.
	</li>
	<ul>
		<li><b>Overwrite</b> - In this mode the signal generator output overwrites the input.</li>
		<li><b>Add</b> - In this mode the signal generator output is summed to the input.</li>
		<li><b>Multiply</b> - In this mode the signal generator output is multiplied to the input.</li>
	</ul>
	<li><b>SOLO</b> - Whether the channel should be solo.</li>
	<li><b>MUTE</b> - Whether the channel should be mute.</li>
	<li><b>In</b> Level - The level of the input signal.</li>
	<li><b>1, 2, ...</b> Level - The level of each of the channels.</li>
	<li><b>Out</b> Level - The level of the output signal.</li>
	<li><b>In</b> Meter - A level meter for each of the input channels.</li>
	<li><b>Out</b> Meter - A level meter for each of the output channels.</li>
	<li><b>Input</b> Level - This control allows to tune a global input gain applied to all channels.</li>
	<li><b>Output</b> Level - This control allows to tune a global output gain applied to all channels.</li>
</ul>

<p><b>'Analisys' section:</b></p>
<p>
 	This section allows to tune the settings of the FFT analyser used to produce the spectra in the
 	<b>'SPECTRUM GRAPH' section</b>. These control apply to all channels.
</p>
<ul>
	<li><b>Reactivity</b> - The analyser reactivity, in milliseconds.</li>
	<li><b>Shift</b> - A vertical shift for the spectra plots, in decibels.</li>
</ul>

<p><b>'Generators' section:</b></p>
<p>
 	This section allows to tune the noise generator parameters. There is a set of controls for each channel.
 	Many of these controls are activated depending on the value of other controls. A lot of these control have
 	a subtle effect on the noise quality. The effect can be more or less evident depending on the value of the
 	other controls.
</p>
<ul>
	<li><b>Amp</b> - Amplitude of the noise generator.</li>
	<li><b>Offs</b> - Offset of the noise generator.</li>
	<li><b>Settings</b> - General settings for the generator. The following are available:</li>
	<ul>
		<li><b>S</b> (Solo) - Whether this generator should be solo.</li>
		<li><b>M</b> (Mute) - Whether this generator should be mute.</li>
		<li>
			<b>Inaudible</b> - Whether this generator should output inaudible noise. When active,
			this control will set the noise to white and pass it through a high pass filter with cutoff
			frequency at 24 kHz. For this to work the sample rate must be higher than 48 kHz. For sample
			rates lower than this the noise cannot be made inaudible.
		</li>
	</ul>
	<li><b>Type</b> - This control selects the type of noise generator. The following types are available.</li>
	<ul>
		<li>
			<b>Off</b> - Shuts off the noise generator. 
		</li>
		<li>
			<b>MLS</b> - Maximum Length Sequence. This is a high quality generator with ideal decorrelation.
			It is the "most random" noise available. However, it has only two states (samples are emitted only
			at minimal or maximum range).
		</li>
		<li>
			<b>LCG</b> - Linear Congruental Generator. This is the simplest generator. It's main feature is
			that it allows to tune the statistical distribution of the noise. See <b>Distribution</b>.
		</li>
		<li>
			<b>Velvet</b> - Velvet Noise Generator. This is a sparse noise generator (random pulses
			separated by random amounts of silence). It has specific types (see <b>Velvet Type</b>)
			and it can be crushed. Crushing means that the spike values are rounded to 0 or full scale
			depending on the outcome of a random variable. This process can be tuned by a probability
			to which the random variable is compared.
		</li>
	</ul>
	<li><b>Color</b> - Color of the noise. The following colors are available:</li>
	<ul>
		<li><b>White</b> - Uniform frequency weighting.</li>
		<li><b>Pink</b> - 3 dB per octave attenuation.</li>
		<li><b>Red</b> - 6 dB per octave attenuation.</li>
		<li><b>Blue</b> - 3 dB per octave gain.</li>
		<li><b>Violet</b> - 6 dB per octave gain.</li>
		<li>
			<b>Custom (Np/Np)</b> - Custom color in units of Neper per Neper. 
			Set with the <b>Cstm</b> knob.
		</li>
		<li>
			<b>Custom (dB/Octave)</b> - Custom color in units of decibels per octave. 
			Set with the <b>Cstm</b> knob.
		</li>
		<li>
			<b>Custom (dB/decade)</b> - Custom color in units of decibels per decade. 
			Set with the <b>Cstm</b> knob.
		</li>
	</ul>
	<li><b>Cstm</b> - Custom color value. Only active for custom colors. See <b>Color</b>.</li>
	<li>
		<b>Distribution</b> - Statistical distribution. Applies only to <b>LCG</b> noise. See <b>Type</b>.
		The distributions below are available.
	</li>
	<ul>
		<li><b>Uniform</b> - A uniform probability distribution.</li>
		<li><b>Exponential</b> - A two sided exponential probability distribution.</li>
		<li><b>Triangular</b> - A triangular probability distribution.</li>
		<li><b>Gaussian</b> - A gaussian probability distribution.</li>
	</ul>
	<li>
		<b>Velvet Type</b> - The type of Velvet Noise. Applies only to <b>Velvet</b> noise. See <b>Type</b>.
		The types are based on those described in <b>GENERALIZATIONS OF VELVET NOISE AND THEIR USE IN 1-BIT MUSIC</b>
		by Kurt James Werner. All Velvet noise types can be crushed. They all depend on the <b>Window</b> parameter.
		The following types are available:
	</li>
	<ul>
		<li><b>OVN</b> - Original Velvet Noise.</li>
		<li><b>OVNA</b> - Original Velvet Noise, Alternative implementation.</li>
		<li><b>ARN</b> - Additive Random Noise. This type of velvet noise is additionally controlled by <b>ARN Delta</b>.</li>
		<li><b>TRN</b> - Totally Random Noise.</li>
	</ul>
	<li>
		<b>Window</b> - The Velvet noise window, in seconds. This control the (staistical) time between velvet
		noise spikes (or, the average duration of the random silence periods between spikes). This control is
		active only for <b>Velvet</b> noise. See See <b>Type</b>.
	</li>
	<li>
		<b>ARN Delta</b> - The ARN Velvet noise delta. For <b>ARN</b> <b>Velvet</b> Noise, this controls a tradeoff between advancing
		time by a fixed amount (0) and a random amount (1). This control is active only for <b>ARN</b> <b>Velvet</b> noise. See <b>Type</b>
		and <b>Velvet Type</b>.
	</li>
	<li>
		<b>Crush</b> - Whether to activate crushing. Only applies to <b>Velvet</b> noise. See <b>Type</b>.
		The crushing operation rounds the values of the Velvet noise spikes based on a probability set by
		<b>Crush prob</b>.
	</li>
	<li>
		<b>Crush Prob</b> - The Velvet noise crushing probability. Applies only if the noise is <b>Velvet</b> (see <b>Type</b>) 
		and <b>Crush</b> is on. This control affect the probability by which the velvet noise spikes are rounded to full
		scale when crushing is active. In most cases the effect of this control will be subtle unless near an extreme.
		However, how subtle this control is depends on all the other Velvet noise settings.
	</li>
</ul>