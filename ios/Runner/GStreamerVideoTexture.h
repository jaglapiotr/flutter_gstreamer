#pragma once

#import <Flutter/Flutter.h>

NS_ASSUME_NONNULL_BEGIN

// Thin Objective-C shim exposing SyntheticVideoSource (synthetic_video_source.h/.cpp, plain
// C++) through the FlutterTexture protocol. Kept as a plain Objective-C interface (no C++
// types) so it can be imported from Swift via Runner-Bridging-Header.h.
@interface GStreamerVideoTexture : NSObject <FlutterTexture>

// Marked nonnull (via NS_ASSUME_NONNULL) so Swift imports this as a non-failable init.
- (instancetype)initWithTextureRegistry:(id<FlutterTextureRegistry>)registry
                                   width:(int)width
                                  height:(int)height;

// Stops the pipeline and unregisters the texture. Safe to call more than once.
- (void)dispose;

@property(nonatomic, readonly) int64_t textureId;

@end

NS_ASSUME_NONNULL_END
