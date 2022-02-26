if exists('b:did_ftplugin')
  finish
endif

let s:cpo = &cpo
set cpo&vim

setl lisp
setl comments=:;;;;,:;;;,:;;,:;,sr:#\|,mb:\|,ex:\|#
setl commentstring=;%s
setl define=^\\s*(def\\k*
setl iskeyword=33,35-39,42-43,45-58,60-90,94,95,97-122,126

let b:undo_ftplugin = 'setl lisp< comments< commentstring< define< iskeyword<'

setl lispwords+=define
setl lispwords+=setq
setl lispwords+=quote
setl lispwords+=cond
setl lispwords+=and
setl lispwords+=or
setl lispwords+=lambda
setl lispwords+=setf
setl lispwords+=load
setl lispwords+=begin
setl lispwords+=unquote
setl lispwords+=qsquote
setl lispwords+=unqtesp
setl lispwords+=try
setl lispwords+=catch
setl lispwords+=getf
setl lispwords+=szof
setl lispwords+=flsym

let b:undo_ftplugin = b:undo_ftplugin . ' lispwords<'

let b:did_ftplugin = 1
let &cpo = s:cpo
unlet s:cpo
