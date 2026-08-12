#import "GStreamerVideoTexture.h"

#include "synthetic_video_source.h"

#include <memory>

@interface GStreamerVideoTexture () {
  std::unique_ptr<SyntheticVideoSource> _source;
}
@property(nonatomic, weak) id<FlutterTextureRegistry> registry;
@end

@implementation GStreamerVideoTexture

- (instancetype)initWithTextureRegistry:(id<FlutterTextureRegistry>)registry
                                   width:(int)width
                                  height:(int)height {
  self = [super init];
  if (self == nil) {
    return nil;
  }

  _registry = registry;
  _textureId = [registry registerTexture:self];
  NSLog(@"KANAPKA GStreamerVideoTexture: registered textureId=%lld, size=%dx%d", _textureId, width, height);

  // Called from the GStreamer streaming thread, not the main thread.
  __weak GStreamerVideoTexture* weakSelf = self;
  _source = std::make_unique<SyntheticVideoSource>(width, height, [weakSelf]() {
    GStreamerVideoTexture* strongSelf = weakSelf;
    if (strongSelf != nil) {
      [strongSelf.registry textureFrameAvailable:strongSelf.textureId];
    }
  });

  return self;
}

- (void)dispose {
  _source.reset();
  [_registry unregisterTexture:_textureId];
}

- (void)dealloc {
  [self dispose];
}

#pragma mark - FlutterTexture

- (CVPixelBufferRef)copyPixelBuffer {
  CVPixelBufferRef buffer = _source ? _source->CopyLatestPixelBuffer() : NULL;
  static int callCount = 0;
  int count = ++callCount;
  if (count <= 5 || count % 60 == 0) {
    NSLog(@"KANAPKA GStreamerVideoTexture: copyPixelBuffer call #%d, buffer=%p", count, (void*)buffer);
  }
  return buffer;
}

@end
