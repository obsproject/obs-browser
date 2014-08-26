/******************************************************************************
 Copyright (C) 2014 by John R. Bradley <jrb@turrettech.com>

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

#pragma once

#import <Foundation/Foundation.h>

#include <stdint.h>
#include <string>

#include <pthread.h>

#include <obs-module.h>
#include <util/platform.h>
#include <obs-output.h>

class TextureRef;

class BrowserSource {

public:
	BrowserSource(obs_data_t settings, obs_source_t source);
	~BrowserSource();

public:
	void UpdateSettings(obs_data_t settings);
	void UpdateBrowser();

public:
	const std::string& GetUrl() const { return url; }
	uint32_t GetWidth() const { return width; }
	uint32_t GetHeight() const { return height; }
	obs_source_t GetSource() const { return source; }
	bool HasValidBrowserIdentifier() const
		{ return hasValidBrowserIdentifier; }

	int GetBrowserIdentifier() { return browserIdentifier; }

	TextureRef *GetCurrentRenderTexture() const;
	void SetCurrentRenderTexture(TextureRef *texture);

	void LockTexture() { pthread_mutex_lock(&textureLock); }
	void UnlockTexture() { pthread_mutex_unlock(&textureLock); }

	gs_effect_t GetDrawEffect() { return drawEffect; }
	void SetDrawEffect(gs_effect_t drawEffect)
		{ this->drawEffect = drawEffect; }

	gs_samplerstate_t GetSampler() { return sampler; }
	void SetSampler(gs_samplerstate_t sampler)
		{ this->sampler = sampler; }

	gs_vertbuffer_t GetVertexBuffer() { return vertexBuffer; }
	void SetVertexBuffer(gs_vertbuffer_t vertexBuffer)
		{ this->vertexBuffer = vertexBuffer; }

private:
	obs_source_t source;
	std::string url;
	uint32_t width;
	uint32_t height;
	uint32_t fps;

	bool hasValidBrowserIdentifier;
	int browserIdentifier;

	TextureRef *currentRenderTexture;
	pthread_mutex_t textureLock;

	gs_effect_t drawEffect;
	gs_samplerstate_t sampler;
	gs_vertbuffer_t vertexBuffer;

};
