TEMPLATE= subdirs

MAKEFILE= Makefile.all

SUBDIRS= lightmaps_builder  q2_loader q3_loader
lightmaps_builder.file= panzer_lightmaps_builder.pro
3_loader.file= q3_loader.pro
q2_loader.file= q2_loader.pro

lightmaps_builder.depends= q2_loader q3_loader
