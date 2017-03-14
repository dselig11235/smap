;;;; This file is part of Smap.
;;;; Copyright (C) 2010, 2014 Sergey Poznyakoff
;;;;
;;;; Smap is free software; you can redistribute it and/or modify
;;;; it under the terms of the GNU General Public License as published by
;;;; the Free Software Foundation; either version 3, or (at your option)
;;;; any later version.
;;;;
;;;; Smap is distributed in the hope that it will be useful,
;;;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;;;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;;;; GNU General Public License for more details.
;;;;
;;;; You should have received a copy of the GNU General Public License
;;;; along with Smap.  If not, see <http://www.gnu.org/licenses/>.

;;;; This script is a sample Guile module for smapd.  It implements two
;;;; maps:
;;;;   user NAME   - return the passwd entry for NAME
;;;;   groups NAME - return the list of group names for the user NAME.

(use-modules (srfi srfi-1))

;;; This function is called after by the smap subprocess before
;;; entering main loop.
(define (smap-open dbname . rest)
  ;; Everything you write to the standard error port is sent to the
  ;; smapd error output.
  (format (current-error-port) "smap-open called; dbname=~A, arguments: ~A~%"
	  dbname rest)
  #t)

;;; This function is called after by the smap subprocess after
;;; exiting from the event loop, and before terminating.
(define (smap-close handle)
  (format (current-error-port) "smap-close called~%"))

(define (query-user name)
  (let ((pw (getpwnam name)))
    (string-append
     "OK "
     (passwd:name pw)
     ":"
     (passwd:passwd pw)
     ":"
     (number->string (passwd:uid pw))
     ":"
     (number->string (passwd:gid pw))
     ":"
     (passwd:gecos pw)
     ":"
     (passwd:dir pw)
     ":"
     (passwd:shell pw))))

(define (query-groups name)
  (setgrent)
  (fold
   (lambda (elem prev)
     (string-append prev " " elem))
   "OK"
   (let loop ((gl (list (group:name (getgrnam (passwd:gid (getpwnam name))))))
	      (gr (getgrent)))
     (cond
      (gr
       (loop (if (member name (group:mem gr))
		 (cons (group:name gr) gl)
		gl)
	     (getgrent)))
      (else
       (endgrent)
       gl)))))


(define map-list
  (list
   (cons "user" query-user)
   (cons "groups" query-groups)))

;;; Smap-query is called to handle each query.  Arguments are:
;;;   handle -  database handle returned by smap_open
;;;   map    -  the map name
;;;   arg    -  query arguments
;;;   rest   -  connection information
(define-public (smap-query handle map arg . rest)
  ;; Log the connection
  (let ((src (car rest)))
    (format (current-error-port) "connect from ~A~%"
	    (if (= (sockaddr:fam src) AF_INET)
		(inet-ntoa (sockaddr:addr src))
		"UNIX socket")))
  ;; Select appropriate handler and call it
  (let ((elt (assoc map map-list)))
    ;; Whatever you write to the current output port, is buffered until
    ;; the newline character, then converted into a proper `sockmap'
    ;; protocol packet and sent back to the client as a reply.
    (display
     (cond
      (elt
       (catch 'misc-error
	      (lambda ()
		((cdr elt) arg))
	      (lambda args
		"NOTFOUND")))
      (else
       (format (current-error-port) "unknown map name: ~A~%" map)
       "NOTFOUND")))
    (newline)))

(define-public (smap-xform handle arg . rest)
  (let ((arg-parts (string-split arg #\@)))
    (if (null? (cdr arg-parts))
	#f
	(car arg-parts))))

;;; Module initialization function returns an associative list
;;; of methods implemented by the module.  Each method is represented
;;; by a cons.
(define (init dbname)
  (list
   (cons "query" smap-query)
   (cons "xform" smap-xform)
   (cons "open" smap-open)
   (cons "close" smap-close)))

;;;; End of getpw.scm
