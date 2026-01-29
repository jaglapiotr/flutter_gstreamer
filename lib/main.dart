import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

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
      });

      print("!!! Received textureId: $textureId");
    } catch (e) {
      print("!!! Error: $e");
    }
  }

  @override
  void dispose() {
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text("Windows Native Texture PoC")),
      body: Center(
        child: _textureId == null
            ? const CircularProgressIndicator()
            : Container(
                width: 640,
                height: 360,
                decoration: BoxDecoration(
                  border: Border.all(color: Colors.blue, width: 6),
                ),
                child: Texture(textureId: _textureId!),
              ),
      ),
    );
  }
}
