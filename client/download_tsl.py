import sys
import shutil
import os
import xml.etree.ElementTree as ET
import urllib.request
import urllib.error
import urllib.parse
import time

def download_tsl_file(url, output_path, filename):
    output_file = os.path.join(output_path, filename)
    print(f"Downloading TSL list from: {url}")
    retries = 3
    for attempt in range(retries):
        try:
            with urllib.request.urlopen(url, timeout=60) as response, open(output_file, 'wb') as f:
                shutil.copyfileobj(response, f)
            print(f"Saved to: {output_file}")
            return output_file
        except (urllib.error.HTTPError, urllib.error.URLError) as e:
            print(f"Attempt {attempt + 1}/{retries} failed: {e}")
            if attempt + 1 < retries:
                sleep_time = 10 * (attempt + 1)
                print(f"Retrying in {sleep_time} seconds...")
                time.sleep(sleep_time)
            else:
                print("All retry attempts failed.")
                sys.exit(1)
        except IOError as e:
            print(f"Error saving TSL list {output_file}: {e}")
            sys.exit(1)

def download_tsl_files(output_path, list_url, territories):
    master_list_filename = os.path.basename(urllib.parse.urlparse(list_url).path)
    if not master_list_filename:
        master_list_filename = "eu-lotl.xml" # Fallback filename
    file = download_tsl_file(list_url, output_path, master_list_filename)
    try:
        root = ET.parse(file)
        ns = {'tsl': "http://uri.etsi.org/02231/v2#"}
        for pointer in root.findall('.//tsl:OtherTSLPointer', ns):
            territory = pointer.find('.//tsl:SchemeTerritory', ns)
            if territory is None or territory.text not in territories:
                continue

            mime = pointer.find('.//tslx:MimeType', {'tslx': 'http://uri.etsi.org/02231/v2/additionaltypes#'})
            if mime is None or mime.text != 'application/vnd.etsi.tsl+xml':
                continue

            location = pointer.find('.//tsl:TSLLocation', ns)
            if location is not None:
                download_tsl_file(location.text, output_path, f"{territory.text}.xml")

    except ET.ParseError as e:
        print(f"Error parsing XML: {e}")
        sys.exit(1)

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python download_tsl.py <output_path> <url> <territory1> [territory2] ...")
        sys.exit(1)

    output_path = sys.argv[1]
    url = sys.argv[2]
    territories_to_download = sys.argv[3:]

    if not os.path.isdir(output_path):
        print(f"Error: Output path '{output_path}' is not a directory.")
        sys.exit(1)

    download_tsl_files(output_path, url, territories_to_download)
    print("TSL download process finished.")
