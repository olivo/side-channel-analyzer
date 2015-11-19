<?php

// Helper functions for handling Stmts for PHP-Parser.

include_once "PHP-Parser-master/lib/bootstrap.php";

// Gets the string representation of the LHS variable
// in an assignment.

function getVarFromExprAssignment($exprAssign) {

	 $var = $exprAssign->var;
	 return $var->name;
 }

// Prints a sequence of statements with a pretty printer.

function printStmts($stmts) {
	 $prettyPrinter = new PhpParser\PrettyPrinter\Standard;
	 $code = $prettyPrinter->prettyPrint($stmts);
	 print $code."\n";
 }





?>