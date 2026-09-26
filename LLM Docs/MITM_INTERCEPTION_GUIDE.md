# MITM Interception Guide for Quill Theater App

This guide documents how to intercept HTTPS traffic from the Meta Quest Quill Theater app to capture GraphQL metadata and .imm file downloads.

## Overview

The Quill Theater app on Meta Quest communicates with Facebook's GraphQL API to fetch gallery metadata (titles, creators, thumbnails) and downloads .imm animation files from CDN servers. We can intercept this traffic using mitmproxy.

## Prerequisites

- Windows PC with Python installed
- Meta Quest headset on the same network
- mitmproxy installed (`pip install mitmproxy`)

## Setup Steps

### 1. Install mitmproxy Certificate on Quest

The Quest needs to trust mitmproxy's CA certificate to allow HTTPS interception.

```bash
# Start mitmproxy once to generate certificates
mitmproxy

# Certificate location (Windows):
# C:\Users\<username>\.mitmproxy\mitmproxy-ca-cert.cer
```

**Install certificate on Quest:**
1. Copy `mitmproxy-ca-cert.cer` to Quest via USB or SideQuest
2. On Quest: Settings > Security > Install from storage
3. Select the certificate file
4. Name it "mitmproxy" and install as CA certificate

### 2. Configure Quest Proxy Settings

On Quest:
1. Settings > Wi-Fi > Select your network > Advanced
2. Set Proxy to "Manual"
3. Proxy hostname: Your PC's IP address (e.g., 192.168.1.100)
4. Proxy port: 8080

### 3. Start mitmproxy with Recording

```bash
# Record all traffic to a .mitm file for later analysis
mitmweb -w quill_capture.mitm

# Or use mitmdump for headless capture:
mitmdump -w quill_capture.mitm
```

### 4. Launch Quill Theater and Browse

1. Open Quill Theater app on Quest
2. Browse the gallery - this triggers GraphQL API calls
3. Open individual animations - this downloads .imm files
4. Let it run for a while to capture as much as possible

## Traffic Patterns

### GraphQL API Requests (Metadata)

**Endpoint:** `https://graph.facebook.com/graphql`

**Request format:** POST with form-encoded body containing:
- `fb_api_req_friendly_name`: Operation name (e.g., `QuillGalleryItemsRefetchQuery`)
- `variables`: JSON with pagination cursors
- `doc_id`: Query document ID

**Response format:** JSON containing:
```json
{
  "data": {
    "mediaStudioGallery": {
      "gallery_items": {
        "edges": [
          {
            "node": {
              "id": "...",
              "media_studio_item": {
                "id": "1234567890",
                "name": "Animation Title",
                "creator": {
                  "media_creator_name": "Artist Name"
                },
                "imm_file": {
                  "uri": "https://scontent.xx.fbcdn.net/..."
                }
              }
            }
          }
        ]
      }
    }
  }
}
```

**Key fields for metadata extraction:**
- `media_studio_item.id` - The item ID (used in filenames)
- `media_studio_item.name` - Animation title
- `media_studio_item.creator.media_creator_name` - Creator name
- `media_studio_item.imm_file.uri` - Download URL for .imm file

### IMM File Downloads

**CDN URLs:** `https://scontent.xx.fbcdn.net/...` (various CDN hostnames)

**File format:** Binary .imm files (Quill's animation format)

## Extracting Data from .mitm Files

### Method 1: Python Script with mitmproxy Library

```python
from mitmproxy import io as mitmio
from mitmproxy.exceptions import FlowReadException
import json

def extract_from_mitm(mitm_file):
    metadata = {}
    imm_files = []
    
    with open(mitm_file, "rb") as f:
        reader = mitmio.FlowReader(f)
        try:
            for flow in reader.stream():
                request = flow.request
                response = flow.response
                
                if response is None:
                    continue
                
                # Extract GraphQL responses (metadata)
                if "graph.facebook.com" in request.host and "/graphql" in request.path:
                    try:
                        data = json.loads(response.content)
                        # Parse gallery items from response
                        extract_gallery_items(data, metadata)
                    except:
                        pass
                
                # Extract IMM file downloads
                if response.headers.get("content-type", "").startswith("application/octet-stream"):
                    if len(response.content) > 10000:  # Likely an IMM file
                        imm_files.append({
                            "url": request.url,
                            "size": len(response.content),
                            "content": response.content
                        })
        
        except FlowReadException as e:
            print(f"Error reading flow: {e}")
    
    return metadata, imm_files

def extract_gallery_items(data, metadata):
    """Recursively search for gallery items in GraphQL response"""
    if isinstance(data, dict):
        # Check if this is a media_studio_item node
        if "media_studio_item" in data:
            item = data["media_studio_item"]
            if item and "id" in item:
                item_id = item["id"]
                title = item.get("name", "Unknown")
                creator = "Unknown"
                if item.get("creator"):
                    creator = item["creator"].get("media_creator_name", "Unknown")
                
                metadata[item_id] = {
                    "item_id": item_id,
                    "title": title,
                    "creator": creator
                }
        
        # Recurse into nested objects
        for value in data.values():
            extract_gallery_items(value, metadata)
    
    elif isinstance(data, list):
        for item in data:
            extract_gallery_items(item, metadata)
```

### Method 2: Export to HAR and Parse

```bash
# Convert .mitm to HAR format
mitmdump -r capture.mitm --set hardump=./capture.har
```

Then parse the HAR file (standard JSON format) with any JSON parser.

## Saving IMM Files from MITM Capture

```python
def save_imm_files(mitm_file, output_dir):
    """Extract and save IMM files from MITM capture"""
    import os
    import hashlib
    
    with open(mitm_file, "rb") as f:
        reader = mitmio.FlowReader(f)
        for flow in reader.stream():
            response = flow.response
            if response is None:
                continue
            
            content_type = response.headers.get("content-type", "")
            if "octet-stream" in content_type and len(response.content) > 50000:
                # Generate filename from URL hash or extract from headers
                url_hash = hashlib.md5(flow.request.url.encode()).hexdigest()[:12]
                filename = f"{url_hash}.imm"
                
                filepath = os.path.join(output_dir, filename)
                with open(filepath, "wb") as out:
                    out.write(response.content)
                print(f"Saved: {filename} ({len(response.content)} bytes)")
```

## Troubleshooting

### Certificate Issues
- Quest may need a reboot after installing the certificate
- Some system apps ignore user-installed certificates
- Check certificate is installed under "User credentials" in Quest settings

### Connection Issues
- Ensure PC firewall allows incoming connections on port 8080
- Verify Quest and PC are on same subnet
- Try disabling VPN on both devices

### Missing Traffic
- Not all traffic may be captured if app uses certificate pinning
- Some requests may use HTTP/2 or other protocols
- Gallery pagination means you need to scroll to trigger more API calls

## Files in This Project

- `quill_flows_auto.mitm` - Main MITM capture file (47MB)
- `all_mitm_metadata.json` - Extracted metadata (423 items with title/creator)
- `extract_mitm_metadata.py` - Script to extract metadata from .mitm files
- `rename_from_mitm.py` - Script to rename .imm files using extracted metadata

## Key Learnings

1. **Certificate installation is critical** - Without the CA cert, HTTPS traffic cannot be decrypted
2. **GraphQL responses contain nested data** - Must recursively search for `media_studio_item` nodes
3. **Creator field path** - The creator name is at `media_studio_item.creator.media_creator_name`, NOT `creator.username`
4. **Pagination** - Gallery loads items in batches; must scroll to capture all metadata
5. **IMM files are large** - Typically 500KB-50MB, so captures grow quickly

## Rate Limiting Note

The Facebook GraphQL API has rate limits. If making direct API calls (not through interception), expect throttling after ~50-100 requests. The MITM approach captures data as the user naturally browses, avoiding rate limits.
