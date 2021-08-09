# Not internal things




# Internal to Microchip (should probably not be included in a github readme)

## Setup required for development of the library

Firstly, grab (DxCore)[https://github.com/SpenceKonde/DxCore/blob/master/Installation.md].

Thereafter, one has to clone this repo to one's Arduino library (folder)[https://www.arduino.cc/en/hacking/libraries]. 

For cryptoauthlib, we need build the archive, copy it and the header files to the `src` folder. This is done by calling the `inject_cryptoauthlib.sh` script. The `clear_cryptoauthlib.sh` removes all the cryptoauthlib related files from the source directory. This is a somewhat awkward setup, but we do this because of three things:

1. Arduino doesn't allow us to specify include paths from the library (it's fixed at the source folder), so we have to 'inject' the headers in the source folder and not some sub folder.
2. Compile time is reduced significantly by using an archive for cryptoauthlib. There are a lot of source files in the cryptoauthlib, and having them be compiled each time the user uploads the sketch will slow down development significantly.
3. Easier to use for the user. In this way, the library can just be downloaded and used. We only need to make sure that we do the 'injecting' before we push the library upstream or update it.

The scripts have dependencies on bash, cmake and make.

The board needs to be provisioned, which there are some details about below in the security profile and certificates section.

After that, one can run one of the examples in `src/examples`. Open one of them up in the Arduino IDE, modify your setup from the `tools` menu to set the board, chip and port and upload the sketch. If it complains about TWI1, look below.


### Things which need to be merged into DxCore

- Currently there are no TWI1 support in DxCore. This is begin worked on in one of the issues and should be patched in soon. For making this to work at the moment you need to copy the contents of the patch in this (issue)[https://github.com/SpenceKonde/DxCore/issues/54#issuecomment-860186363] by MX682X. Copy his source files to `<place where DxCore is located>/DxCore/hardware/megaavr/x.x.x/libraries/Wire/src`.
- Static linking support, hopefully this will be merged when this goes into someone other's hands. The PR is (here)[https://github.com/SpenceKonde/DxCore/pull/128]. If it is not yet upstream, you need to replace the platform.txt in that PR with the one in DxCore's root for cryptoauthlib to link correctly.


## Security profile and certificates

### MQTT 

For secure MQTT with TLS, we utilise the ATECC. In order to get this communication to work we have to extract the certificate already stored on the ATECC and add them to the cloud provider/MQTT broker. The procedure is:

1. Extract device certificate (specific for ATECC608B and ATCECC508A)

There is example code for this under the example folder, which will both the device certificate and signer certificate, which can be used. It grabs the public key by using atcab_getpubkey. Then it grabs the certificates by using atcacert_read_cert. Grabbing the certificate requires a certificate definition which can be found in the example folder. The certificate is in x509 format and can after extraction be uploaded to the MQTT broker. 

2. Register the device certificate in the sequans module and for the broker. `AT+SQNSNVW="certificate",0,<certificate length><CR><LF><certificate data>` for storing the device certificate in the sequans module. For registering with the broker this is different from broker to broker, but for AWS this is done in the IoT console (google is your friend here).  *might also need to register signer certificate, not sure about this*. 

3. Store CA certificate for the MQTT broker on the Sequans module in slot 19 (arbitrary, but this is what the library uses). This can be done with the `AT+SQNSNVW="certificate",19,<certificate length><CR><LF><certificate data>` command.

4. Enable this configuration: `AT+SQNSPCFG=1,2,"0xC02B",5,19,0,0,"","",1`. Have a look at Seqans' AT command reference for more details around this command. 


### HTTPS 

For HTTPS we just use the bundled CA certificate(s) stored in the Sequans module. To make the security profile, issue the following: `AT+SQNSPCFG=2,,,5,1`. We utilise security profile ID 2 for HTTPS, since we reserve 1 to MQTT.





# Todo

## ECC
- There is some race condition with hal_i2c where the if we don't use enough 
  time, it just halts. Need the delay for now.


## HTTPS
- Some hostnames still doesn't work... Might be something with CA


## MQTT
- Ability to not use ATECC? -> Store private key on Sequans module 


## Sequans Controller


## General
- Strings in progmem?


## DxCore 
