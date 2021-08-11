# injects devicename into platform.io OTA environment. 



DEVICE_NAME  = 'DEVICE_NAME'

with open('src/device_name.h') as f:
    lines = f.readlines()

    for line in lines:
        if DEVICE_NAME in line:
            line = line.replace(' ','') # remove spaces
            line = line.replace(';','')
            line = line.replace('"','')
            device_name = line.split('=')[-1]

print(device_name+'.local')