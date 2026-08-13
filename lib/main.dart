import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter/foundation.dart';

void main() {
  runApp(const MaterialApp(home: TextureTestScreen()));
}

class TextureTestScreen extends StatefulWidget {
  const TextureTestScreen({super.key});

  @override
  State<TextureTestScreen> createState() => _TextureTestScreenState();
}

class _TextureTestScreenState extends State<TextureTestScreen> {
  static const platform = MethodChannel('win_texture_poc');
  int? _textureId;
  final TextEditingController _osdTextController = TextEditingController();

  @override
  void initState() {
    super.initState();
    _initializeTexture();
  }

  Future<void> _initializeTexture() async {
    try {
      final int textureId = await platform.invokeMethod('createTexture');

      setState(() {
        _textureId = textureId;
        print("!!! Received textureId repaint: $textureId");
      });

      print("!!! Received textureId: $textureId");
    } catch (e) {
      print("!!! Error: $e");
    }
  }

  Future<void> _setOsdText() async {
    try {
      await platform.invokeMethod('setOsdText', {
        'text': _osdTextController.text,
      });
    } catch (e) {
      print("!!! Error setting OSD text: $e");
    }
  }

  @override
  void dispose() {
    _osdTextController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text("Windows Native Texture PoC"),
        actions: [
          IconButton(
            iconSize: 60,
            icon: const Icon(Icons.add),
            onPressed: () {
              debugPrint('Button pressed');
            },
          ),
        ],
      ),
      body: Center(
        child: _textureId == null
            ? const CircularProgressIndicator()
            : Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Container(
                    width: 640,
                    height: 360,
                    decoration: BoxDecoration(
                      border: Border.all(color: Colors.blue, width: 6),
                    ),
                    child: Texture(textureId: _textureId!),
                  ),
                  const SizedBox(height: 16),
                  Padding(
                    padding: const EdgeInsets.symmetric(horizontal: 24),
                    child: Row(
                      children: [
                        Expanded(
                          child: TextField(
                            controller: _osdTextController,
                            decoration: const InputDecoration(
                              labelText: 'OSD text',
                              border: OutlineInputBorder(),
                            ),
                            onSubmitted: (_) => _setOsdText(),
                          ),
                        ),
                        const SizedBox(width: 8),
                        ElevatedButton(
                          onPressed: _setOsdText,
                          child: const Text('Set OSD'),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
      ),
    );
  }
}
