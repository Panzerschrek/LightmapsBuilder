TEMPLATE= subdirs

MAKEFILE= Makefile.all

SUBDIRS= \
	lightmaps_builder \
	q1_loader \
	q2_loader \
	q3_loader \
	hl_loader \

lightmaps_builder.file= panzer_lightmaps_builder.pro
q1_loader.file= q1_loader.pro
q2_loader.file= q2_loader.pro
q3_loader.file= q3_loader.pro
hl_loader.file= hl_loader.pro

lightmaps_builder.depends= \
	q2_loader \
	q2_loader \
	q3_loader \
	hl_loader \
