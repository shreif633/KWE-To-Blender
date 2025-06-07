#
# GTX Texture Converter: gtx_converter.py
#
import os
from .util import BinaryReader, BinaryWriter, SwordCrypt

def convert_gtx_to_dds(gtx_path, dds_path, game_path):
    """
    Converts a Kal Online .gtx texture to a standard .dds file.
    """
    if not os.path.exists(gtx_path):
        print(f"GTX file not found: {gtx_path}")
        return False

    crypt = SwordCrypt(game_path)
    if not crypt.loaded:
        print("Cannot convert GTX without encryption tables.")
        return False

    os.makedirs(os.path.dirname(dds_path), exist_ok=True)

    with open(gtx_path, 'rb') as f_in, open(dds_path, 'wb') as f_out:
        reader = BinaryReader(f_in)
        writer = BinaryWriter(f_out)
        
        # The first 3 bytes are "GTX", we write "DDS "
        reader.read_bytes(3)
        writer.write_bytes(b'DDS ')

        # Bytes 3 to 7 are copied directly (total 4 bytes)
        writer.write_bytes(reader.read_bytes(4))
        
        # Bytes 8 to 71 are decrypted with key $04 (total 64 bytes)
        for _ in range(64):
            val = reader.read_byte()
            decrypted_val = crypt.decrypt(val, 0x04)
            writer.write_byte(decrypted_val)
            
        # The rest of the file is copied directly
        rest_of_file = reader.read_bytes(-1) # Read to end
        writer.write_bytes(rest_of_file)
        
    print(f"Converted {os.path.basename(gtx_path)} to DDS.")
    return True