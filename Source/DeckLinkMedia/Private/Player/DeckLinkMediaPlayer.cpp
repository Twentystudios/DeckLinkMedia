// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaPrivate.h"
#include "DeckLinkMediaPlayer.h"

#include "HAL/FileManager.h"
#include "IMediaEventSink.h"
#include "IMediaOptions.h"
#include "MediaSamples.h"
#include "IMediaTextureSample.h"
#include "Misc/ScopeLock.h"

#include "DeckLinkMediaTextureSample.h"
#include "DeckLink/DecklinkDevice.h"


#define LOCTEXT_NAMESPACE "FDeckLinkMediaPlayer"


/* FDeckLinkMediaPlayer structors
 *****************************************************************************/

FDeckLinkMediaPlayer::FDeckLinkMediaPlayer( IMediaEventSink& InEventSink, const TMap<uint8, TUniquePtr<DeckLinkDevice>>* Devices )
	: Samples( new FMediaSamples )
	, CurrentState( EMediaState::Closed )
	, CurrentTime( FTimespan::Zero() )
	, EventSink( InEventSink )
	, SelectedAudioTrack( INDEX_NONE )
	, SelectedMetadataTrack( INDEX_NONE )
	, SelectedVideoTrack( INDEX_NONE )
	, CurrentDim( FIntPoint::ZeroValue )
	, CurrentFps( 0.0 )
	, DeviceMap( Devices )
	, CurrentDeviceIndex( 0 )
	, Paused( false )
	, UseFrameTimecode( false )
	, VideoSampleFormat( EMediaTextureSampleFormat::CharBGRA )
{

}


FDeckLinkMediaPlayer::~FDeckLinkMediaPlayer()
{
	Close();

	delete Samples;
	Samples = nullptr;
}


/* IMediaControls interface
 *****************************************************************************/

bool FDeckLinkMediaPlayer::CanControl( EMediaControl Control ) const
{
	if( Control == EMediaControl::Pause )
	{
		return ( CurrentState == EMediaState::Playing );
	}

	if( Control == EMediaControl::Resume )
	{
		return ( CurrentState == EMediaState::Paused );
	}

	return false;
}

FTimespan FDeckLinkMediaPlayer::GetDuration() const
{
	return (CurrentState == EMediaState::Playing) ? FTimespan::MaxValue() : FTimespan::Zero();
}


float FDeckLinkMediaPlayer::GetRate() const
{
	return (CurrentState == EMediaState::Playing) ? 1.0f : 0.0f;
}


EMediaState FDeckLinkMediaPlayer::GetState() const
{
	return CurrentState;
}



EMediaStatus FDeckLinkMediaPlayer::GetStatus() const
{
	return ( CurrentState == EMediaState::Preparing ) ? EMediaStatus::Connecting : EMediaStatus::None;
}


TRangeSet<float> FDeckLinkMediaPlayer::GetSupportedRates( EMediaRateThinning /*Thinning*/ ) const
{
	TRangeSet<float> Result;

	Result.Add( TRange<float>( 0.0f ) );
	Result.Add( TRange<float>( 1.0f ) );

	return Result;
}


FTimespan FDeckLinkMediaPlayer::GetTime() const
{
	return CurrentTime;
}


bool FDeckLinkMediaPlayer::IsLooping() const
{
	return false; // not supported
}


bool FDeckLinkMediaPlayer::Seek( const FTimespan& Time )
{
	return false; // not supported
}


bool FDeckLinkMediaPlayer::SetLooping( bool Looping )
{
	return false; // not supported
}


bool FDeckLinkMediaPlayer::SetRate( float Rate )
{
	if( Rate == 0.0f )
	{
		Paused = true;
	}
	else if( Rate == 1.0f )
	{
		Paused = false;
	}
	else
	{
		return false;
	}

	return true;
}


/* IMediaPlayer interface
 *****************************************************************************/

void FDeckLinkMediaPlayer::Close()
{
	{
		FScopeLock Lock(&CriticalSection);
		const auto& Device = (*DeviceMap)[CurrentDeviceIndex];
		if( Device && Device->IsCapturing() ) {
			Device->Stop();
			Device->ReadFrameCallback( nullptr );
		}

		CurrentFps = 0.0f;
		CurrentState = EMediaState::Closed;
		CurrentUrl.Empty();
		CurrentDim = FIntPoint::ZeroValue;

		SelectedAudioTrack = INDEX_NONE;
		SelectedMetadataTrack = INDEX_NONE;
		SelectedVideoTrack = INDEX_NONE;
	}

	EventSink.ReceiveMediaEvent( EMediaEvent::TracksChanged );
	EventSink.ReceiveMediaEvent( EMediaEvent::MediaClosed );
}

IMediaCache& FDeckLinkMediaPlayer::GetCache()
{
	return *this;
}

IMediaControls& FDeckLinkMediaPlayer::GetControls()
{
	return *this;
}


FString FDeckLinkMediaPlayer::GetInfo() const
{
	return Info;
}


FName FDeckLinkMediaPlayer::GetPlayerName() const
{
	static FName PlayerName(TEXT("DeckLinkMedia"));
	return PlayerName;
}

IMediaSamples& FDeckLinkMediaPlayer::GetSamples()
{
	return *Samples;
}

FString FDeckLinkMediaPlayer::GetStats() const
{
	FString StatsString;
	{
		StatsString += TEXT("not implemented yet");
		StatsString += TEXT("\n");
	}

	return StatsString;
}


IMediaTracks& FDeckLinkMediaPlayer::GetTracks()
{
	return *this;
}


FString FDeckLinkMediaPlayer::GetUrl() const
{
	return CurrentUrl;
}

IMediaView& FDeckLinkMediaPlayer::GetView()
{
	return *this;
}


bool FDeckLinkMediaPlayer::Open( const FString& Url, const IMediaOptions*  Options )
{
	if( Url.IsEmpty() || !Url.StartsWith( TEXT( "sdi://" ) ) )
	{
		return false;
	}

	const TCHAR* DeviceNumberStr = &Url[12];
	auto NewIndex = FCString::Atoi( DeviceNumberStr ) - 1;
	if( NewIndex < 0 || NewIndex >= DeviceMap->Num() ) {
		UE_LOG( LogDeckLinkMedia, Error, TEXT( "Invalid device id." ) );
		return false;
	}
	CurrentDeviceIndex = NewIndex;

	Close();

	const auto& Device = (*DeviceMap)[CurrentDeviceIndex];
	auto Mode = BMDDisplayMode::bmdModeHD1080p2398;
	Device->Start( Mode );

	// finalize
	{
		FScopeLock Lock(&CriticalSection);
		CurrentDim = Device->GetCurrentSize();
		CurrentFps = Device->GetCurrentFps();
		VideoSampleFormat = EMediaTextureSampleFormat::CharBGRA;
		CurrentState = EMediaState::Stopped;
		CurrentUrl = Url;
	}

	// notify listeners
	EventSink.ReceiveMediaEvent( EMediaEvent::TracksChanged );
	EventSink.ReceiveMediaEvent( EMediaEvent::MediaOpened );

	return true;
}


bool FDeckLinkMediaPlayer::Open( const TSharedRef<FArchive, ESPMode::ThreadSafe>& /*Archive*/, const FString& /*OriginalUrl*/, const IMediaOptions* /*Options*/ )
{
	return false; // not supported
}


void FDeckLinkMediaPlayer::TickFetch( FTimespan DeltaTime, FTimespan Timecode )
{
	QUICK_SCOPE_CYCLE_COUNTER( STAT_DeckLinkMediaPlayer_TickVideo );

	if( ( CurrentState == EMediaState::Playing ) && ( SelectedVideoTrack == 0 ) )
	{
		auto TextureSample = MakeShared<FDeckLinkMediaTextureSample, ESPMode::ThreadSafe>( VideoSampleFormat, CurrentTime, FTimespan::FromSeconds(  1.0f / CurrentFps ) );
		auto newFrame = ( *DeviceMap )[CurrentDeviceIndex]->GetFrame( &TextureSample->Frame );
		if( newFrame ) {
			Samples->AddVideo( TextureSample );
		}
	}

	//TODO: Handle timecode
	CurrentTime += DeltaTime;
}


void FDeckLinkMediaPlayer::TickInput( FTimespan DeltaTime, FTimespan Timecode )
{
	const EMediaState State = Paused ? EMediaState::Paused : EMediaState::Playing;

	if( State != CurrentState )
	{
		CurrentState = State;
		EventSink.ReceiveMediaEvent( State == EMediaState::Playing ? EMediaEvent::PlaybackResumed : EMediaEvent::PlaybackSuspended );
	}
}


/* IMediaTracks interface
 *****************************************************************************/

bool FDeckLinkMediaPlayer::GetAudioTrackFormat( int32 TrackIndex, int32 FormatIndex, FMediaAudioTrackFormat& OutFormat ) const
{
	return 0; // not supported
}

int32 FDeckLinkMediaPlayer::GetNumTracks(EMediaTrackType TrackType) const
{
	if ( (*DeviceMap)[CurrentDeviceIndex] != nullptr ) // @todo fixme
	{
		if (TrackType == EMediaTrackType::Video)
		{
			return 1;
		}
	}

	return 0;
}

int32 FDeckLinkMediaPlayer::GetNumTrackFormats( EMediaTrackType TrackType, int32 TrackIndex ) const
{
	return ( ( TrackIndex == 0 ) && ( GetNumTracks( TrackType ) > 0 ) ) ? 1 : 0;
}



int32 FDeckLinkMediaPlayer::GetSelectedTrack(EMediaTrackType TrackType) const
{
	if( (*DeviceMap)[CurrentDeviceIndex] == nullptr )
	{
		return INDEX_NONE;
	}

	switch( TrackType )
	{
	case EMediaTrackType::Audio:
	case EMediaTrackType::Metadata:
		return 0;
	case EMediaTrackType::Video:
		return SelectedVideoTrack;
	default:
		return INDEX_NONE;
	}
}


FText FDeckLinkMediaPlayer::GetTrackDisplayName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	if( (*DeviceMap)[CurrentDeviceIndex] == nullptr || (TrackIndex != 0) )
	{
		return FText::GetEmpty();
	}

	switch( TrackType )
	{
	case EMediaTrackType::Audio:
		return LOCTEXT( "DefaultAudioTrackName", "Audio Track" );

	case EMediaTrackType::Metadata:
		return LOCTEXT( "DefaultMetadataTrackName", "Metadata Track" );

	case EMediaTrackType::Video:
		return LOCTEXT( "DefaultVideoTrackName", "Video Track" );

	default:
		return FText::GetEmpty();
	}
}

int32 FDeckLinkMediaPlayer::GetTrackFormat( EMediaTrackType TrackType, int32 TrackIndex ) const
{
	return ( GetSelectedTrack( TrackType ) != INDEX_NONE ) ? 0 : INDEX_NONE;
}

FString FDeckLinkMediaPlayer::GetTrackLanguage(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return TEXT("und");
}


FString FDeckLinkMediaPlayer::GetTrackName(EMediaTrackType TrackType, int32 TrackIndex) const
{
	return FString();
}

bool FDeckLinkMediaPlayer::GetVideoTrackFormat( int32 TrackIndex, int32 FormatIndex, FMediaVideoTrackFormat& OutFormat ) const
{
	if( ( TrackIndex != 0 ) || ( FormatIndex != 0 ) )
	{
		return false;
	}

	OutFormat.Dim = CurrentDim;
	OutFormat.FrameRate = CurrentFps;
	OutFormat.FrameRates = TRange<float>( CurrentFps );
	OutFormat.TypeName = ( VideoSampleFormat == EMediaTextureSampleFormat::CharBGRA ) ? TEXT( "UYVY" ) : TEXT( "RGBA" );

	return true;
}


bool FDeckLinkMediaPlayer::SelectTrack(EMediaTrackType TrackType, int32 TrackIndex)
{
	if ((TrackIndex != INDEX_NONE) && (TrackIndex != 0))
	{
		return false;
	}

	if (TrackType == EMediaTrackType::Video)
	{
		SelectedVideoTrack = TrackIndex;
	}
	else
	{
		return false;
	}

	return true;
}

bool FDeckLinkMediaPlayer::SetTrackFormat( EMediaTrackType TrackType, int32 TrackIndex, int32 FormatIndex )
{
	if( ( TrackIndex != 0 ) || ( FormatIndex != 0 ) )
	{
		return false;
	}

	return ( TrackType == EMediaTrackType::Video );
}

#undef LOCTEXT_NAMESPACE
