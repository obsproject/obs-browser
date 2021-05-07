/******************************************************************************
 Copyright (C) 2019 by Hugh Bailey ("Jim") <jim@obsproject.com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "obs-browser-source.hpp"
#if CHROME_VERSION_BUILD < 4103 && CHROME_VERSION_BUILD >= 3683
void BrowserSource::EnumAudioStreams(obs_source_enum_proc_t cb, void *param)
{
	std::lock_guard<std::mutex> lock(audio_sources_mutex);
	for (obs_source_t *audio_source : audio_sources) {
		cb(source, audio_source, param);
	}
}

static inline void mix_audio(float *__restrict p_out,
			     const float *__restrict p_in, size_t pos,
			     size_t count)
{
	float *__restrict out = p_out;
	const float *__restrict in = p_in + pos;
	const float *__restrict end = in + count;

	while (in < end)
		*out++ += *in++;
}

bool BrowserSource::AudioMix(uint64_t *ts_out,
			     struct audio_output_data *audio_output,
			     size_t channels, size_t sample_rate)
{
	uint64_t timestamp = 0;
	struct obs_source_audio_mix child_audio;

	std::lock_guard<std::mutex> lock(audio_sources_mutex);
	for (obs_source_t *s : audio_sources) {
		if (!obs_source_audio_pending(s)) {
			uint64_t source_ts = obs_source_get_audio_timestamp(s);

			if (source_ts && (!timestamp || source_ts < timestamp))
				timestamp = source_ts;
		}
	}

	if (!timestamp)
		return false;

	for (obs_source_t *s : audio_sources) {
		uint64_t source_ts;
		size_t pos, count;

		if (obs_source_audio_pending(s)) {
			continue;
		}

		source_ts = obs_source_get_audio_timestamp(s);
		if (!source_ts) {
			continue;
		}

		pos = (size_t)ns_to_audio_frames(sample_rate,
						 source_ts - timestamp);
		count = AUDIO_OUTPUT_FRAMES - pos;

		obs_source_get_audio_mix(s, &child_audio);
		for (size_t ch = 0; ch < channels; ch++) {
			float *out = audio_output->data[ch];
			float *in = child_audio.output[0].data[ch];

			mix_audio(out, in, pos, count);
		}
	}

	*ts_out = timestamp;
	return true;
}
#endif
