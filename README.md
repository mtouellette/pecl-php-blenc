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
