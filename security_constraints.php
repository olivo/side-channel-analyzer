<?php

	require "PHP-Parser-master/lib/bootstrap.php";
	
	// Generates a map of the expression constraints that 
	// arrive to critical points in the code for each line.

	function generate_security_constraints($stmts) {
	       
	       $line_constraints = array();

	       // Generate the constraints to reach an inner insertion
	       // for each of the main statements.
	       foreach($stmts as $stmt)
	       	       generate_security_constraints_rec($stmt,
					     $line_constraints,
					     array());
					
		return $line_constraints;
	}	

	// Computes the constraints from the root of $stmt to 
	// a database insertion.
	function generate_security_constraints_rec($stmt,
						   $line_constraints,
						   $current_constraints) {


		if($stmt instanceof PhpParser\Node\Stmt\If_) {
			 echo "Found an IF!\n";
			 echo "The key value pairs are:\n";
			 foreach($stmt as $key => $value)
			 	       echo $key."\n";;
			 
		}
		else {
//			echo "NOT Stmt!\n";
}
//			echo "Its type is ".($stmt->getType())."\n";
				



   }


?>