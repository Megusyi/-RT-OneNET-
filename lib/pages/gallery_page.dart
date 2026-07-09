// Copyright (c) 2026, Megusyi, Lkkk8990
//
// SPDX-License-Identifier: Apache-2.0
//
// lib/pages/gallery_page.dart
import 'package:flutter/material.dart';
import 'package:photo_view/photo_view.dart';
import '../config/app_config.dart';
import '../services/onenet_service.dart';

class GalleryPage extends StatefulWidget {
  const GalleryPage({super.key});

  @override
  State<GalleryPage> createState() => _GalleryPageState();
}

class _GalleryPageState extends State<GalleryPage> {
  final OneNetService _service = OneNetService();
  List<Map<String, dynamic>> _images = [];
  Map<String, String> _urlCache = {};
  bool _loading = true;
  String? _error;

  @override
  void initState() {
    super.initState();
    _loadImages();
  }

  Future<void> _loadImages() async {
    setState(() {
      _loading = true;
      _error = null;
    });
    try {
      final list = await _service.getImageList();
      setState(() {
        _images = list;
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _error = e.toString();
        _loading = false;
      });
    }
  }

  String _getDownloadUrl(String fid) {
    if (_urlCache.containsKey(fid)) return _urlCache[fid]!;
    final url = _service.getImageDownloadUrl(fid);
    _urlCache[fid] = url;
    return url;
  }

  void _viewFullScreen(String url, String fileName) {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder: (_) => Scaffold(
          backgroundColor: Colors.black,
          appBar: AppBar(
            backgroundColor: Colors.black,
            iconTheme: const IconThemeData(color: Colors.white),
            title: Text(
              fileName,
              style: const TextStyle(color: Colors.white, fontSize: 16),
              overflow: TextOverflow.ellipsis,
            ),
            actions: [],
          ),
          body: PhotoView(
            imageProvider: NetworkImage(url),
            minScale: PhotoViewComputedScale.contained,
            maxScale: PhotoViewComputedScale.covered * 3,
            backgroundDecoration: const BoxDecoration(color: Colors.black),
            loadingBuilder: (_, event) => const Center(
              child: CircularProgressIndicator(color: Colors.white),
            ),
            errorBuilder: (_, error, __) => const Center(
              child: Icon(Icons.broken_image, color: Colors.white54, size: 64),
            ),
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    if (_loading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (_error != null) {
      return Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            const Icon(Icons.error_outline, size: 48, color: Colors.red),
            const SizedBox(height: 12),
            Text(_error!, style: const TextStyle(color: Colors.red)),
            const SizedBox(height: 12),
            ElevatedButton(onPressed: _loadImages, child: const Text('重试')),
          ],
        ),
      );
    }

    if (_images.isEmpty) {
      return const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.photo_library_outlined, size: 64, color: Colors.grey),
            SizedBox(height: 12),
            Text('暂无照片', style: TextStyle(fontSize: 16, color: Colors.grey)),
          ],
        ),
      );
    }

    return RefreshIndicator(
      onRefresh: _loadImages,
      child: GridView.builder(
        padding: const EdgeInsets.all(8),
        gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
          crossAxisCount: 3,
          crossAxisSpacing: 8,
          mainAxisSpacing: 8,
        ),
        itemCount: _images.length,
        itemBuilder: (context, index) {
          final file = _images[index];
          final fid = file['fid']?.toString() ?? '';
          final fileName = file['name']?.toString() ?? '未知';
          final url = _getDownloadUrl(fid);

          return GestureDetector(
            onTap: () {
              if (url.isNotEmpty) _viewFullScreen(url, fileName);
            },
            child: Card(
              clipBehavior: Clip.antiAlias,
              child: Stack(
                fit: StackFit.expand,
                children: [
                  Image.network(
                    url,
                    fit: BoxFit.cover,
                    headers: {'authorization': AppConfig.userAuthorization},
                    loadingBuilder: (_, child, progress) {
                      if (progress == null) return child;
                      return const Center(
                        child: CircularProgressIndicator(strokeWidth: 2),
                      );
                    },
                    errorBuilder: (_, __, ___) => const Center(
                      child: Icon(Icons.broken_image, color: Colors.grey),
                    ),
                  ),
                  Positioned(
                    bottom: 0,
                    left: 0,
                    right: 0,
                    child: Container(
                      color: Colors.black54,
                      padding: const EdgeInsets.all(4),
                      child: Text(
                        fileName,
                        style: const TextStyle(
                          color: Colors.white,
                          fontSize: 11,
                        ),
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                      ),
                    ),
                  ),
                ],
              ),
            ),
          );
        },
      ),
    );
  }
}