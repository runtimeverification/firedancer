$(call add-hdrs,fd_shred.h)
$(call add-objs,fd_shred,fd_ballet)
$(call make-unit-test,test_shred,test_shred,fd_ballet fd_util)
$(call make-unit-test,test_deshredder,test_deshredder,fd_ballet fd_util)
