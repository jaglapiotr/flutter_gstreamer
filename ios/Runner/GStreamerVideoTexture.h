#pragma once

#import <Flutter/Flutter.h>

// Thin Objective-C shim exposing SyntheticVideoSource (synthetic_video_source.h/.cpp, plain
// C++) through the FlutterTexture protocol. Kept as a plain Objective-C interface (no C++
// types) so it can be imported from Swift via Runner-Bridging-Header.h.
@interface GStreamerVideoTexture : NSObject <FlutterTexture>

- (instancetype)initWithTextureRegistry:(id<FlutterTextureRegistry>)registry
                                   width:(int)width
                                  height:(int)height;

// Stops the pipeline and unregisters the texture. Safe to call more than once.
- (void)dispose;

@property(nonatomic, readonly) int64_t textureId;

@end
