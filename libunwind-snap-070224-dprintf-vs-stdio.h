--- libunwind-0.99-alpha-orig/include/libunwind_i.h	2006-07-28 05:30:50.000000000 +0200
+++ libunwind-0.99-alpha/include/libunwind_i.h	2008-09-22 01:53:04.000000000 +0200
@@ -238,11 +238,11 @@ extern int unwi_dyn_validate_cache (unw_
 extern unw_dyn_info_list_t _U_dyn_info_list;
 extern pthread_mutex_t _U_dyn_info_list_lock;
 
+#include <stdio.h>
 #if UNW_DEBUG
 #define unwi_debug_level		UNWI_ARCH_OBJ(debug_level)
 extern long unwi_debug_level;
 
-# include <stdio.h>
 # define Debug(level,format...)						\
 do {									\
   if (unwi_debug_level >= level)					\
