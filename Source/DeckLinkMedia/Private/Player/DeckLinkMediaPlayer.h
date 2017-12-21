// Copyright 2017 The Mill, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IMediaCache.h"
#include "IMediaControls.h"
#include "IMediaPlayer.h"
#include "IMediaTracks.h"
#include "IMediaView.h"

class FMediaSamples;
class DeckLinkDevice;
class IMediaEventSink;
enum class EMediaTextureSampleFormat;

/**
 * Implements a media player EXR image sequences.
 */
class FDeckLinkMediaPlayer
	: public IMediaPlayer
	, protected IMediaCache
	, protected IMediaControls
	, protected IMediaTracks
	, protected IMediaView
{
public:

	/** Default constructor. */
	FDeckLinkMediaPlayer( IMediaEventSink& InEventSink, const TMap<uint8, TUniquePtr<DeckLinkDevice>>* Devices );

	/** Destructor. */
	~FDeckLinkMediaPlayer();

public:
	//~ IMediaControls interface

	virtual bool CanControl( EMediaControl Control ) const override;
	virtual FTimespan GetDuration() const override;
	virtual float GetRate() const override;
	virtual EMediaState GetState() const override;
	virtual EMediaStatus GetStatus() const override;
	virtual TRangeSet<float> GetSupportedRates( EMediaRateThinning Thinning ) const override;
	virtual FTimespan GetTime() const override;
	virtual bool IsLooping() const override;
	virtual bool Seek( const FTimespan& Time ) override;
	virtual bool SetLooping( bool Looping ) override;
	virtual bool SetRate( float Rate ) override;

public:

	//~ IMediaPlayer interface

	virtual void Close() override;
	virtual IMediaCache& GetCache() override;
	virtual IMediaControls& GetControls() override;
	virtual FString GetInfo() const override;
	virtual FName GetPlayerName() const override;
	virtual IMediaSamples& GetSamples() override;
	virtual FString GetStats() const override;
	virtual IMediaTracks& GetTracks() override;
	virtual FString GetUrl() const override;
	virtual IMediaView& GetView() override;
	virtual bool Open(const FString& Url, const IMediaOptions* Options) override;
	virtual bool Open( const TSharedRef<FArchive, ESPMode::ThreadSafe>& Archive, const FString& OriginalUrl, const IMediaOptions* Options ) override;
	virtual void TickFetch( FTimespan DeltaTime, FTimespan Timecode ) override;
	virtual void TickInput( FTimespan DeltaTime, FTimespan Timecode ) override;

public:

	//~ IMediaTracks interface

	virtual bool GetAudioTrackFormat( int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat ) const override;
	virtual int32 GetNumTracks( EMediaTrackType TrackType ) const override;
	virtual int32 GetNumTrackFormats( EMediaTrackType TrackType, int32 TrackIndex ) const override;
	virtual int32 GetSelectedTrack( EMediaTrackType TrackType ) const override;
	virtual FText GetTrackDisplayName( EMediaTrackType TrackType, int32 TrackIndex ) const override;
	virtual int32 GetTrackFormat( EMediaTrackType TrackType, int32 TrackIndex ) const override;
	virtual FString GetTrackLanguage( EMediaTrackType TrackType, int32 TrackIndex ) const override;
	virtual FString GetTrackName( EMediaTrackType TrackType, int32 TrackIndex ) const override;
	virtual bool GetVideoTrackFormat( int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat ) const override;
	virtual bool SelectTrack( EMediaTrackType TrackType, int32 TrackIndex ) override;
	virtual bool SetTrackFormat( EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex ) override;

private:

	/** Index of the selected audio track. */
	int32 SelectedAudioTrack;

	/** Index of the selected metadata track. */
	int32 SelectedMetadataTrack;

	/** Index of the selected video track. */
	int32 SelectedVideoTrack;

private:

	/** Critical section for synchronizing access to receiver and sinks. */
	FCriticalSection CriticalSection;

	/** Video dimensions of the last processed frame image. */
	FIntPoint CurrentDim;

	/** Frames per second of the currently opened sequence. */
	float CurrentFps;

	/** Current state of the media player. */
	EMediaState CurrentState;

	/** The current time of the playback. */
	FTimespan CurrentTime;

	/** The URL of the currently opened media. */
	FString CurrentUrl;

	/** The media event handler. */
	IMediaEventSink& EventSink;

	/** The duration of the media. */
    FTimespan Duration;

	/** Media information string. */
	FString Info;

	/** Whether the player is paused. */
	bool Paused;

	/** Whether to use the time code embedded in NDI frames. */
	bool UseFrameTimecode;

	/** The media sample cache. */
	FMediaSamples* Samples;

	/** The current video sample format. */
	EMediaTextureSampleFormat VideoSampleFormat;

	const TMap<uint8, TUniquePtr<DeckLinkDevice>> *			DeviceMap;
	int32													CurrentDeviceIndex;
};
