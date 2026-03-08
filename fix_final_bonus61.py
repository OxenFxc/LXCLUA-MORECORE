# Wait, my fix60.py grep'ed for `return mp` and printed context!
# And it outputted `vmp_example` stuff because the `test_bonus_small2.lua` output was NOT the file being tested!
# Let me look at the very end of `output_test_bonus_small7.log`!
# `vmp_example ( 100 )  :7362: warning:`
# Wait!
# Why is `vmp_example` STILL in `lexer_cff_vmp_v12.lua`?!
# I thought I deleted `vmp_example` from `lexer_cff_vmp_v12.lua`!
# OH. I ran `git checkout lexer_cff_vmp_v12.lua` earlier, which REVERTED the file to the ORIGINAL version containing `vmp_example`!
# So ALL my fixes applied to `lexer_cff_vmp_v12.lua` (including `collect_locals`, fixing `safe_split_statements`, fixing `__M[get_bonus]`) WERE WIPED OUT BY `git checkout` !!!
# No wonder it was still outputting `vmp_example`!
