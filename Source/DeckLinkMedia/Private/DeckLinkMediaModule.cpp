// Copyright 2017 The Mill, Inc. All Rights Reserved.

#include "DeckLinkMediaPrivate.h"

#include "IDeckLinkMediaModule.h"
#include "Modules/ModuleManager.h"
#include "DeckLinkMediaPlayer.h"

#include "HAL/PlatformProcess.h"
#include "IPluginManager.h"
#include "Paths.h"

#include "DeckLink/DecklinkDevice.h"

DEFINE_LOG_CATEGORY( LogDeckLinkMedia );

#define LOCTEXT_NAMESPACE "FDeckLinkMediaModule"

/**
 * Implements the DeckLinkMedia module.
 */
class FDeckLinkMediaModule
	: public IDeckLinkMediaModule
{
public:
	/** Default constructor. */
	FDeckLinkMediaModule()
	: Initialized( false ) { }

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	void DeviceArrived( IDeckLink* decklink, size_t id );
	virtual TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> CreatePlayer( IMediaEventSink& EventSink ) override;
private:
	TSharedPtr<DeckLinkDeviceDiscovery> DeviceDiscovery;
	TMap<uint8, TUniquePtr<DeckLinkDevice>> DeviceMap;

	/** Whether the module has been initialized. */
	bool Initialized;
};

IMPLEMENT_MODULE(FDeckLinkMediaModule, DeckLinkMedia);

void FDeckLinkMediaModule::StartupModule()
{
	using namespace std::placeholders;
	auto DeviceCallback = std::bind( &FDeckLinkMediaModule::DeviceArrived, this, _1, _2 );
	DeviceDiscovery.Reset();
	DeviceDiscovery = MakeShareable( new DeckLinkDeviceDiscovery( DeviceCallback ) );

	Initialized = true;
}

void FDeckLinkMediaModule::ShutdownModule()
{
	if( ! Initialized )
	{
		return;
	}

	Initialized = false;

	DeviceMap.Empty();
	DeviceDiscovery.Reset();
}

void FDeckLinkMediaModule::DeviceArrived( IDeckLink* decklink, size_t id )
{
	uint8 DeviceId = static_cast<uint8>(id);
	DeviceMap.Emplace( DeviceId, MakeUnique<DeckLinkDevice>( DeviceDiscovery.Get(), decklink ) );
	UE_LOG( LogDeckLinkMedia, Log, TEXT( "Device %d arrived." ), id );

	DeviceMap.KeySort( []( uint8 A, uint8 B ) {
		return A < B;
	} );
}

TSharedPtr<IMediaPlayer, ESPMode::ThreadSafe> FDeckLinkMediaModule::CreatePlayer( IMediaEventSink& EventSink )
{
	if( ! Initialized )
	{
		return nullptr;
	}

	return MakeShared<FDeckLinkMediaPlayer, ESPMode::ThreadSafe>( EventSink, &DeviceMap );
}


#undef LOCTEXT_NAMESPACE
