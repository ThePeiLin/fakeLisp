## specials: ??, ?e, ?symbol, ?s-dint, ?s-xint, ?s-oint, ?s-char ...

*s-exp* -> *list*       =>  first
        -> *vector*     =>  first
        -> *box*        =>  first
        -> *hasheq*     =>  first
        -> *hasheqv*    =>  first
        -> *hashequal*  =>  first
        -> *quote*      =>  first
        -> *qsquote*    =>  first
        -> *unquote*    =>  first
        -> *unqtesp*    =>  first
        -> *symbol*     =>  first
        -> *string*     =>  first
        -> *bytes*      =>  first
        -> *integer*    =>  first
        -> *float*      =>  first
        -> *char*       =>  first

*quote*   -> "'" *s-exp*   =>  quote
*qsquote* -> "`" *s-exp*   =>  qsquote
*unquote* -> "~" *s-exp*   =>  unquote
*unqtesp* -> "~@" *s-exp*  =>  unqtesp

*symbol* -> ?symbol["|"]       =>  symbol
*string* -> /""|"(\\.|.)*"/    =>  string
*bytes*  -> /#""|#"(\\.|.)*"/  =>  bytes

*integer* -> ?s-dint["|"]  =>  dec_integer
          -> ?s-xint["|"]  =>  hex_integer
          -> ?s-oint["|"]  =>  oct_integer

*float* -> ?s-dfloat["|"]  =>  float
        -> ?s-xfloat["|"]  =>  float

*char* -> ?s-char["#\\"]  =>  char

*box* -> "#&" *s-exp*  =>  box

*list* -> "(" *list-items* ")"  => second
       -> "[" *list-items* "]"  => second

*list-items* ->                       =>  nil
             -> *s-exp* *list-items*  =>  list
             -> *s-exp* "," *s-exp*   =>  pair

*vector* -> "#(" *vector-items* ")"  =>  vector
         -> "#[" *vector-items* "]"  =>  vector

*vector-items* ->                         =>  nil
               -> *s-exp* *vector-items*  =>  list

*hasheq*    -> "#hash("      *hash-items* ")"  =>  hasheq
            -> "#hash["      *hash-items* "]"  =>  hasheq

*hasheqv*   -> "#hasheqv("   *hash-items* ")"  =>  hasheqv
            -> "#hasheqv["   *hash-items* "]"  =>  hasheqv

*hashequal* -> "#hashequal(" *hash-items* ")"  =>  hashequal
            -> "#hashequal[" *hash-items* "]"  =>  hashequal

*hash-items* ->                                           =>  nil
             -> "(" *s-exp* "," *s-exp* ")" *hash-items*  =>  pair_list
             -> "[" *s-exp* "," *s-exp* "]" *hash-items*  =>  pair_list

?e -> /#!.*\n?/
   -> /;.*\n?/
   -> /\s+/
   ;

?? -> "#!"
   -> ";"
   -> "\""
   -> "#\""
