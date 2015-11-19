<?php

include_once "PHP-Parser-master/lib/bootstrap.php";
include_once "StmtProcessing.php";


// Performs symbolic execution over a main CFG, with auxiliary
// CFGs constructed from function and function signature mappings as parameters.
// The idea is to collect the constraints reaching insertion methods.
function symbolic_execute($main_cfg,$function_cfgs,$function_signatures) {

	 print "Starting symbolic execution.\n";
	 
	 // Recursive execution consists of starting from the entry of the main CFG 
	 // with an initial state, empty path constraints,
	 // and then traversing the instructions of the CFG in sequence, changing the mapped values 
	 // in the state according to assignments and accumulating the path constraints from the conditionals 
	 // in the path constraints (as an implicit conjunction).

	 $state = array();
	 $path_constraints = array();
	 symbolic_execute_rec($main_cfg->entry,$function_cfgs,$function_signatures,$state,$path_constraints);
	       
 }

// Performs a step of the symbolic execution algorithm, updating the $state or the $path_constraints
// accordingly.
function symbolic_execute_rec($current_node,$function_cfgs,$function_signatures,$state,$path_constraints) {

	 if($current_node->is_cond) {
	 	// If it's a conditional node, add the path constraint if it's not a loop.
		// Generate separate path conditions for the true and false branches.
		$true_constraints = array();
		$false_constraints = array();
		
		foreach($path_constraints as $constraint) {
			// WARNING: We might need to clone these!
			$true_constraints[] = $constraint;
			$false_constraints[] = $constraint;
		}
		
		// WARNING: Not adding the loop conditions at this point.
		// If it's not a loop header, add the conditions to each branch.
		if(!$current_node->is_loop_header) {
			$true_constraints[] = $current_node->stmt;
			$false_constraints[] = new PhpParser\Node\Expr\BooleanNot($current_node->stmt);
			
		 }
		 
//		 print "CONDITIONAL SYMBOLIC EXECUTION\n";
		 // Perform symbolic execution on both branches.

		 
		 symbolic_execute_rec($current_node->successors[0],
				      $function_cfgs,
				      $function_signatures,
				      $state,
				      $true_constraints);



		 // False branch.
		 symbolic_execute_rec($current_node->successors[1],
				      $function_cfgs,
				      $function_signatures,
				      $state,
				      $false_constraints);

				      

	 } else {
	   // If it's not a conditional statement, it 
	   // can only have one successor.
	   // Go to the next node if it exists and is not a back-edge.

	   // Note : We're not traversing back-edges to avoid 
	   // infinite loops and are not changing the states by 
	   // assignments -- this needs to be implemented.


/*
	   print "The current path constraints at node:\n";
	   if($current_node->stmt)
		printStmts(array($current_node->stmt));
*/

/*
	   print "The constraints are:\n";
	   print_constraints($path_constraints);
*/
	   // Go to the next successor if it's not a back-edge
	   // and it exists.
	   if(count($current_node->successors)
	     && !$current_node->has_backedge)
	     symbolic_execute_rec($current_node->successors[0],
				      $function_cfgs,
				      $function_signatures,
				      $state,
				      $path_constraints);
//	   else 
//	   	print "Done with processing this path\n";	   	

	     

	 }




}


// Prints the set of path constraints.
function print_constraints($path_constraints) {

	 	 
	 printStmts($path_constraints);
}


?>