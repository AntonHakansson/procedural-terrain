We have converted the images to formats supported by stb_image. The orthophoto was converted from JPEG2000 to JPEG with ImageMagick and then down scaled to decrease memory consumption in the repository. The heightmap was converted from GeoTIFF to 16-bit grayscale PNG with GDAL with the following command:

gdal_translate -ot Int16 -of PNG -scale 0 4096 0 32767 -co worldfile=no L3123F.tif L3123F.png