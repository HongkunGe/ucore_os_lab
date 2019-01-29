OBJPREFIX	:= __objs_

.SECONDEXPANSION:
# -------------------- function begin --------------------

# list all files in some directories: (#directories, #types)
# bootfiles = $(call listf_cc,boot)
#   ==>> call listf,boot,"c S"
#   ==>> filter $(if "c S","%.c %.S",%),$(wildcard boot/*) #if-condition: string not empty,"%.c %.S" is res, else %
#   ==>> filter "%.c %.S", "asm.h bootasm.S bootasm.c" #use the filter function to remove non-matching file names
# 	==>> bootasm.S bootasm.c
listf = $(filter $(if $(2),$(addprefix %.,$(2)),%),\
		  $(wildcard $(addsuffix $(SLASH)*,$(1))))

define do_listf_print
__info__print__ := $(call listf,$(1),$(2))
# $(info [function] listf_print $(__info__print__))
endef

# get .o obj files: (#files[, packet])
#  toobj boot,bootasm bootmain
# ==>> obj/boot/bootasm.o obj/boot/bootmain.o
toobj = $(addprefix $(OBJDIR)$(SLASH)$(if $(2),$(2)$(SLASH)),\
		$(addsuffix .o,$(basename $(1))))

# get .d dependency files: (#files[, packet])
todep = $(patsubst %.o,%.d,$(call toobj,$(1),$(2)))

totarget = $(addprefix $(BINDIR)$(SLASH),$(1))

# change $(name) to $(OBJPREFIX)$(name): (#names)
packetname = $(if $(1),$(addprefix $(OBJPREFIX),$(1)),$(OBJPREFIX))

# target is obj files and dependency files
# $$(call todep,$(1),$(4)) ==>> generate dependency file
# $$(call toobj,$(1),$(4)) ==>> generate object file
# cc compile template, generate rule for dep, obj: (file, cc[, flags, dir])
# https://blog.csdn.net/linuxandroidwince/article/details/75221300  << MT MM
define cc_template
# | order-only prerequisites
# impose a specific ordering on the rules to be invoked
# without forcing the target to be updated if one of those rules is executed.
# $$< the first prerequisite in the first rule for this target
$$(call todep,$(1),$(4)): $(1) | $$$$(dir $$$$@)
	$(V)$(2) -I$$(dir $(1)) $(3) -MM $$< -MT "$$(patsubst %.d,%.o,$$@) $$@"> $$@
$$(call toobj,$(1),$(4)): $(1) | $$$$(dir $$$$@)
	$(V)echo + cc $$<
	$(V)$(2) -I$$(dir $(1)) $(3) -c $$< -o $$@
ALLOBJS += $$(call toobj,$(1),$(4))
endef

# compile file: (#files, cc[, flags, dir])
define do_cc_compile
$$(foreach f,$(1),$$(eval $$(call cc_template,$$(f),$(2),$(3),$(4))))
endef

# add files to packet: (#files, cc[, flags, packet, dir])
define do_add_files_to_packet
# $4=filename: __objs_filename else __objs_
__temp_packet__ := $(call packetname,$(4))
# if $$(origin $$(__temp_packet__)) is equal to undefined
# origin tells you where it came from. fron ENV? file? default?
ifeq ($$(origin $$(__temp_packet__)),undefined)
$$(__temp_packet__) :=
endif
__temp_objs__ := $(call toobj,$(1),$(5))
$$(foreach f,$(1),$$(eval $$(call cc_template,$$(f),$(2),$(3),$(5))))
$$(__temp_packet__) += $$(__temp_objs__)
endef

# add objs to packet: (#objs, packet)
define do_add_objs_to_packet
__temp_packet__ := $(call packetname,$(2))
ifeq ($$(origin $$(__temp_packet__)),undefined)
$$(__temp_packet__) :=
endif
$$(__temp_packet__) += $(1)
# $(info [function] do_add_objs_to_packet __temp_packet__ $(__temp_packet__))
endef

# add packets and objs to target (target, #packes, #objs[, cc, flags])
# $$^ and $$+ evaluate to the list of all prerequisites of rules that have already appeared for the same target
# ($$+ with repetitions and $$^ without)
define do_create_target
__temp_target__ = $(call totarget,$(1))
__temp_objs__ = $$(foreach p,$(call packetname,$(2)),$$($$(p))) $(3)
TARGETS += $$(__temp_target__)
ifneq ($(4),)
$$(__temp_target__): $$(__temp_objs__) | $$$$(dir $$$$@)
	$(V)$(4) $(5) $$^ -o $$@
else
$$(__temp_target__): $$(__temp_objs__) | $$$$(dir $$$$@)
endif
endef

define do_print_target_info
#$(info [function] do_print_target_info __temp_packet__  $(__temp_packet__))
#$(info [function] do_print_target_info __temp_target__  $(__temp_target__))
#$(info [function] do_print_target_info __temp_objs__ $(__temp_objs__))
#$(info [function] do_print_target_info TARGETS $(TARGETS))
#$(info [function] do_print_target_info ALLOBJS $(ALLOBJS))
endef
# finish all
define do_finish_all
ALLDEPS = $$(ALLOBJS:.o=.d)
$$(sort $$(dir $$(ALLOBJS)) $(BINDIR)$(SLASH) $(OBJDIR)$(SLASH)):
	$(V)$(MKDIR) $$@
endef

# --------------------  function end  --------------------
# compile file: (#files, cc[, flags, dir])
cc_compile = $(eval $(call do_cc_compile,$(1),$(2),$(3),$(4)))

# add files to packet: (#files, cc[, flags, packet, dir])
add_files = $(eval $(call do_add_files_to_packet,$(1),$(2),$(3),$(4),$(5)))

# add objs to packet: (#objs, packet)
add_objs = $(eval $(call do_add_objs_to_packet,$(1),$(2)))

# add packets and objs to target (target, #packes, #objs, cc, [, flags])
create_target = $(eval $(call do_create_target,$(1),$(2),$(3),$(4),$(5)))

read_packet = $(foreach p,$(call packetname,$(1)),$($(p)))

add_dependency = $(eval $(1): $(2))

finish_all = $(eval $(call do_finish_all))

# ------- print info -------
listf_print = $(eval $(call do_listf_print,$(1),$(2)))
target_print = $(eval $(call do_print_target_info))
