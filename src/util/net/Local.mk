$(call add-hdrs,fd_eth.h fd_ip4.h)
$(call add-objs,fd_eth,fd_util)
$(call make-unit-test,test_eth,test_eth,fd_util)
$(call make-unit-test,test_ip4,test_ip4,fd_util)