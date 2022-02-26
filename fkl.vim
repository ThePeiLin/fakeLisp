if exists('b:current_syntax')
  finish
endif

let s:cpo = &cpo
set cpo&vim

syn spell notoplevel

syn match fklParentheses "[^ '`\t\n()";]\+"
syn match fklParentheses "[)]"
syn match fklIdentifier /[^ '`\t\n()"|;][^ '`\t\n()"|;]*/

syn region fklString start=/\(\\\)\@<!"/ skip=/\\[\\"]/ end=/"/ contains=@Spell
syn region fklSymbol start=/\(\\\)\@<!|/ skip=/\\[\\|]/ end=/|/
syn match fklByts /#b[0-9a-fA-F]*\>/
syn match fklChr /#\\.[^ `'\t\n\[\]()]*/
syn match fklChr /#\\x[0-9a-fA-F]\+/
syn match fklNumber /[+\-]\?[1-9]\+\.\?[0-9]*e\?[0-9]*\>/
syn match fklHexNumber /[+\-]\?0[xX][0-9a-fA-F]\+\.\?[0-9a-fA-F]*p\?[0-9a-fA-F]*\>/
syn match fklOctNumber /[+\-]\?0[0-7]\+\>/
syn match fklOctNumberError /[+\-]\?0[89]*\>/


syn match fklComment /;.*$/ contains=@Spell

syn cluster fklSyntaxCluster contains=fklFunction,fklSyntax,fklLibrarySyntax,fklPreSyntax
syn region fklQuote matchgroup=fklData start=/'[`']*/ end=/[ \t\n()";]/me=e-1
syn region fklQuote matchgroup=fklData start=/'[`']*"/ skip=/\\[\\"]/ end=/"/
syn region fklQuote matchgroup=fklData start=/'[`']*(/ end=/)/ contains=ALLBUT,fklForm,@fklSyntaxCluster
syn region fklQsquote matchgroup=fklData start=/`[`']*/ end=/[ \t\n()";]/me=e-1
syn region fklQsquote matchgroup=fklData start=/`[`']*"/ skip=/\\[\\"]/ end=/"/
syn region fklQsquote matchgroup=fklData start=/`[`']*(/ end=/)/ contains=ALLBUT,fklForm,@fklSyntaxCluster
syn region fklUnquote matchgroup=fklData start=/\~/ end=/[ \t\n()";]/me=e-1
syn region fklUnquote matchgroup=fklData start=/\~(/ end=/)/
syn region fklUnqtesp matchgroup=fklData start=/\~@(/ end=/[ \t\n()";]/me=e-1
syn region fklUnqtesp matchgroup=fklData start=/\~@(/ end=/)/
syn region fklForm matchgroup=fklParentheses start="(" end=")" contains=ALLBUT
syn region fklQuoteForm matchgroup=fklData start=/(/ end=/)/ contained contains=ALLBUT,fklForm,@fklSyntaxCluster
syn keyword fklLibrarySyntax library
syn keyword fklLibrarySyntax export
syn keyword fklLibrarySyntax import

syn keyword fklPreSyntax defmacro
syn keyword fklPreSyntax deftype

syn keyword fklSyntax define
syn keyword fklSyntax setq
syn keyword fklSyntax quote
syn keyword fklSyntax cond
syn keyword fklSyntax and
syn keyword fklSyntax or
syn keyword fklSyntax lambda
syn keyword fklSyntax setf
syn keyword fklSyntax load
syn keyword fklSyntax begin
syn keyword fklSyntax unquote
syn keyword fklSyntax qsquote
syn keyword fklSyntax unqtesp
syn keyword fklSyntax try
syn keyword fklSyntax catch
syn keyword fklSyntax getf
syn keyword fklSyntax szof
syn keyword fklSyntax flsym

syn keyword fklBuiltInSymbol nil
syn keyword fklBuiltInSymbol stdin
syn keyword fklBuiltInSymbol stdout
syn keyword fklBuiltInSymbol stderr

syn keyword fklFunction car
syn keyword fklFunction cdr
syn keyword fklFunction cons
syn keyword fklFunction append
syn keyword fklFunction atom
syn keyword fklFunction null
syn keyword fklFunction not
syn keyword fklFunction eq
syn keyword fklFunction equal
syn keyword fklFunction =
syn keyword fklFunction +
syn keyword fklFunction 1+
syn keyword fklFunction -
syn keyword fklFunction -1+
syn keyword fklFunction *
syn keyword fklFunction /
syn keyword fklFunction %
syn keyword fklFunction >
syn keyword fklFunction >=
syn keyword fklFunction <
syn keyword fklFunction <=
syn keyword fklFunction chr
syn keyword fklFunction int
syn keyword fklFunction dbl
syn keyword fklFunction str
syn keyword fklFunction sym
syn keyword fklFunction byts
syn keyword fklFunction type
syn keyword fklFunction nth
syn keyword fklFunction length
syn keyword fklFunction apply
syn keyword fklFunction clcc
syn keyword fklFunction file
syn keyword fklFunction read
syn keyword fklFunction getb
syn keyword fklFunction prin1
syn keyword fklFunction putb
syn keyword fklFunction princ
syn keyword fklFunction dll
syn keyword fklFunction dlsym
syn keyword fklFunction argv
syn keyword fklFunction go
syn keyword fklFunction chanl
syn keyword fklFunction send
syn keyword fklFunction recv
syn keyword fklFunction error
syn keyword fklFunction raise
syn keyword fklFunction newf
syn keyword fklFunction delf
syn keyword fklFunction lfdl

hi def link fklComment Comment
hi def link fklData Delimiter
hi def link fklPreSyntax PreProc
hi def link fklLibrarySyntax PreProc
hi def link fklBuiltInSymbol Constant
hi def link fklFunction Function
hi def link fklIdentifier Normal
hi def link fklNumber Number
hi def link fklHexNumber Number
hi def link fklOctNumber Number
hi def link fklByts Number
hi def link fklChr Character
hi def link fklOctNumberError Normal
hi def link fklParentheses Normal
hi def link fklQuote Delimiter
hi def link fklQsquote Delimiter
hi def link fklString String
hi def link fklSymbol Normal
hi def link fklSyntax Statement

let b:did_fkl_syntax=1
if exists('b:current_syntax')
  finish
endif
unlet b:did_fkl_syntax

let b:current_syntax = 'fkl'
let &cpo = s:cpo
unlet s:cpo
