# BLowfish ENCryption for PHP Scripts

BLENC is an extension that permit to protect PHP source scripts with Blowfish Encription.
BLENC hooks into the Zend Engine, allowing for transparent execution of PHP scripts previously
encoded with BLENC.

It is not designed for complete security (it is still possible to disassemble the script
into op codes using a package such as XDebug), however it does keep people out of your code
and make reverse engineering difficult.

Extension have been tested with PHP versions 5.6 - 7.2.

## Please notice
As [upstream project](htps://pecl.php.net/package/BLENC) seems to be stalled this should
be consired as fork of it. *Current code is compatible with original BLENC*.

# How to use it?
Using Blenc it not very well documented and you can mess up your code and you
can't get it back. **So please test with test scripts and then apply to your big commercial project!**
and remember to backup code before encoding.

## TL;DR
**As said above. If you encode and forget your decode key your code is not easily
decoded. As every encoding BLENC also breaks so this is not 100% safe and Using
BLENC only makes getting code exploration slower and hides code from amateurs!
(it is still possible to disassemble the script into op codes using a package such as XDebug)**

First compile as php extension. Install shared object (Windows .DLL and Unix systems .SO) to
your php-extension directory. After that enable extension and encode your files.
Add decoding keys and start using.

### This is example for compiling (You need to have PHP development files)

```# phpize
 # ./configure
 # make
 # cp .libs/blenc.so to /some/where/php7/extensions/
```

## Update INI file
Find where your PHP INI files are and add to blenc.ini file:
```; comment out next line to disable blenc extension in php
extension=blenc.so

; The location where BLENC can find the file containing a list of
; available decryption keys. This file must be readable by web-server.
;
blenc.key_file = "/location/to/conf.d/blenc.keys"
```

## Encode your file
To encode your file you can run blencode.php or make you own better script
```blencode.php script.php``` which should output this kind of output

```
BLENC  blenc_protect starts...
BLENC  blowfish unencrypted key: 61078a970fdab66038c8cc8f12548d73
BLENC  file to encode: script.php
BLENC  backup file : backup/script.php.2018_05_31_10_05_53
BLENC  size of content: 24
BLENC  MD5: 0358c26c0bb6f612d8dc7b004bf68143
BLENC  redistributable key: miWqx+zKq0rnRa2wt4jk8v5ckGBRxhS3in041fUJkWU=
BLENC  redistributable key file key_file.blenc updated.
BLENC  done.
```

Your old unencoded file in at backup-directory and encoded file is renamed script.phpenc (which is symlinked to script.php).
Your encode key it at **blenc.key** which is not encoded and if someone can
get that he or she can decode your scripts at ease! Encoded key is on **key_file.blenc**
which you need to copy **/location/to/conf.d/blenc.keys**.
If you like use some other encryption key of your like then add it before encoding
to blenc.key.

## Running your scripts
If you have added keys to correct place you should be able to run it as before.
**There is no promise made that everything works as expected!**

## Extra things to do
For extra safety main encoding key should be changed in **blenc_protect.h** (It's named BLENC_PROTECT_MAIN_KEY). default is '0123456789abcdef0123456789abcdef'. In **blenc_protect.h** you can also make module expire.
