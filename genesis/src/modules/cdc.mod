##  object.method_name          args rest function                 
    $buffer.length()              1   0   native_buffer_len
    $buffer.retrieve()            2   0   native_buffer_retrieve
    $buffer.append()              2   0   native_buffer_append
    $buffer.replace()             3   0   native_buffer_replace
#    $buffer.insert()              ?   ?   ?
    $buffer.subrange()            2   1   native_buffer_subrange
    $buffer.add()                 2   0   native_buffer_add
    $buffer.truncate()            2   0   native_buffer_truncate
# (remove)    $buffer.tail()                
    $buffer.to_string()           1   0   native_buffer_to_string
    $buffer.to_strings()          1   1   native_buffer_to_strings
    $buffer.from_string()         1   0   native_buffer_from_string
    $buffer.from_strings()        1   1   native_buffer_from_strings
    $dictionary.keys()            1   0   native_dict_keys
    $dictionary.add()             3   0   native_dict_add
    $dictionary.del()             2   0   native_dict_del
    $dictionary.contains()        2   0   native_dict_contains
    $network.hostname()           1   0   native_hostname
    $network.ip()                 1   0   native_ip
    $list.length()                1   0   native_listlen
    $list.subrange()              2   1   native_sublist
    $list.insert()                3   0   native_insert
    $list.replace()               3   0   native_replace
    $list.delete()                2   0   native_delete
    $list.setadd()                2   0   native_setadd
    $list.setremove()             2   0   native_setremove
    $list.union()                 2   0   native_union
    $string.length()              1   0   native_strlen
    $string.subrange()            2   1   native_substr
    $string.explode()             1   1   native_explode
    $string.pad()                 2   1   native_pad
    $string.match_begin()         2   1   native_match_begin
    $string.match_template()      2   0   native_match_template
    $string.match_pattern()       2   0   native_match_pattern
    $string.match_regexp()        2   1   native_match_regexp
#    $string.sed()                 3   0   native_strsed
    $string.replace()             3   0   native_str_replace
    $string.crypt()               1   2   native_str_crypt
    $string.uppercase()           1   0   native_str_uppercase
    $string.lowercase()           1   0   native_str_lowercase
    $string.compare()             2   0   native_str_compare
    $string.format()              1   1   native_str_format
    $sys.next_objnum()            0   0   native_next_objnum
    $sys.status()                 0   0   native_status
    $sys.version()                0   0   native_version
    $time.format()                1   1   native_strftime
