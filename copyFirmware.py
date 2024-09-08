# import shutil
# import os
# # Define the paths for the source and destination files
# src_path = os.path.join(".pio", "build", "esp12e", "RemoteMQ.bin")
# dest_path = os.path.join(
#     "C:\\Users\\m_nag\\Desktop\\Android_Projects\\QueueWifi\\app\\src\\main\\assets", "RemoteMQ.bin")

# # Use shutil to copy the file
# if os.path.exists(src_path):
#     shutil.copy(src_path, dest_path)
#     print("Bin file copied successfully!")
# else:
#     print("Error: Bin file does not exist!")

import shutil
import os
Import("env")

def copy_firmware(source, target, env):
    print("copy_firmware")
    src_path = os.path.join(".pio", "build", "esp12e", "updateM71s.bin")
    dest_path = os.path.join(
        "C:\\Users\\m_nag\\Desktop\\Android_Projects\\\MasjedClockWiFi\\app\\src\\main\\assets", "updateM71s.bin")
    if os.path.exists(src_path):
        shutil.copy(src_path, dest_path)
        print("Bin file copied successfully!")
    else:
        print("Error: Bin file does not exist!")


env.AddPostAction("buildprog", copy_firmware)
print("Copy Firmware Function is Start")



