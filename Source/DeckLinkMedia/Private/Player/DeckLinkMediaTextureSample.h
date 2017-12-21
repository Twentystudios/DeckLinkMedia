
#pragma once

#include "DeckLink/DecklinkDevice.h"
#include "IMediaTextureSample.h"

/**
* Implements a media texture sample for NdiMedia.
*/
class FDeckLinkMediaTextureSample
	: public IMediaTextureSample
{
public:
	/** Default constructor. */
	FDeckLinkMediaTextureSample( EMediaTextureSampleFormat InSampleFormat, FTimespan InTime, FTimespan InDuration )
		: Duration( InDuration )
		, Frame()
		, SampleFormat( InSampleFormat )
		, Time( InTime )
	{

	}
public:

	/** The video frame data. */
	DeckLinkDevice::VideoFrameBGRA Frame;

	//~ IMediaTextureSample interface

	virtual const void* GetBuffer() override
	{
		return Frame.data();
	}

	virtual FIntPoint GetDim() const override
	{
		auto cframe = const_cast<DeckLinkDevice::VideoFrameBGRA *>( &Frame );
		return FIntPoint( cframe->GetRowBytes() / 4, cframe->GetHeight() );
	}

	virtual FTimespan GetDuration() const override
	{
		return Duration;
	}

	virtual EMediaTextureSampleFormat GetFormat() const override
	{
		return SampleFormat;
	}

	virtual FIntPoint GetOutputDim() const override
	{
		auto cframe = const_cast<DeckLinkDevice::VideoFrameBGRA *>( &Frame );
		return FIntPoint( cframe->GetWidth(), cframe->GetHeight() );
	}

	virtual uint32 GetStride() const override
	{
		auto cframe = const_cast<DeckLinkDevice::VideoFrameBGRA *>( &Frame );
		return cframe->GetRowBytes();
	}


#if WITH_ENGINE
	virtual FRHITexture* GetTexture() const override
	{
		return nullptr;
	}
#endif //WITH_ENGINE


	virtual FTimespan GetTime() const override
	{
		return Time;
	}

	virtual bool IsCacheable() const override
	{
		return true;
	}

	virtual bool IsOutputSrgb() const override
	{
		return true;
	}
private:

	/** Duration for which the sample is valid. */
	FTimespan Duration;

	/** Sample format. */
	EMediaTextureSampleFormat SampleFormat;

	/** Sample time. */
	FTimespan Time;
};

