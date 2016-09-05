TEMPLATE= subdirs

MAKEFILE= Makefile.all

SUBDIRS= lightmaps_builder q3_loader
lightmaps_builder.file= panzer_lightmaps_builder.pro
q3_loader.file= q3_loader.pro

lightmaps_builder.depends= q3_loader
