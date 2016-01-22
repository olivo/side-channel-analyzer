<?php

include_once "CFGNode.php";
include_once "CFGNodeCond.php";
include_once "CFGNodeStmt.php";
include_once "PHP-Parser-master/lib/bootstrap.php";
include_once "StmtProcessing.php";
include_once "TaintedVariables.php";

// TODO: Change hardwired notions of taint for a specific application.
// Checks whether an expression is tainted, by checking whether a parameter is a tainted variable or a user input.
function isTainted($expr, $tainted_variables) {

       // For now, checking that the expression is either a function call of 'postGetSession' or a variable already in the tainted set.

       if ($expr instanceof PhpParser\Node\Expr\StaticCall || $expr instanceof PhpParser\Node\Expr\FuncCall) {

       	  print "Analyzing static call for taint\n";       	  
	  $function_name = $expr->name;

	  // The expression is tainted if it invokes a basic user input extraction function.
	  if (strcmp($function_name, 'postGetSessionInt') == 0 || strcmp($function_name, 'postGetSessionString') == 0) {

	     return true;
	  }

	  // The expression is tainted if one of the arguments is tainted.
	  foreach ($expr->args as $arg) {
	  	  
		  if(isTainted($arg->value, $tainted_variables)) {

		  	return true;
		  }
	  }
       }
       else if ($expr instanceof PhpParser\Node\Expr\Variable) {

       	  print "Analyzing variable for taint : " . ($expr->name) . "\n";
       
	  return $tainted_variables->contains($expr->name);
       }
       
       return false;
}

// Performs a flow-sensitive forward taint analysis.
function taint_analysis($main_cfg, $function_cfgs, $function_signatures) {

	 print "Starting Taint Analysis.\n";

	 // Map that contains the set of tainted variables 
	 // per CFG node.
	 $tainted_variables_map = new SplObjectStorage();

	 // Forward flow-sensitive taint-analysis.
	 $entry_node = $main_cfg->entry;
	 $q = new SplQueue();
	 $q->enqueue($entry_node);

	 while (count($q)) {
	       
	       $current_node = $q->dequeue();

	       if (!$tainted_variables_map->contains($current_node)) {

	       	  $tainted_variables_map[$current_node] = new TaintedVariables();
	       }

	       print "Started processing node: \n";
	       $current_node->printCFGNode();

	       $initial_tainted_size = $tainted_variables_map[$current_node]->count();

	       // Check if the current node is a statement node with a 
	       // non-null statement.
	       if (CFGNode::isCFGNodeStmt($current_node) && $current_node->stmt) {

	       	  $stmt = $current_node->stmt;
	       	  // Check to see if the statement is an assigment,
		  // and the right hand side is tainted.
		  if (($stmt instanceof PhpParser\Node\Expr\Assign) && isTainted($stmt->expr, $tainted_variables_map[$current_node]) 
		      && (!$tainted_variables_map[$current_node]->contains($stmt->var->name))) {

		     $tainted_variables_map[$current_node]->attach($stmt->var->name);
		     print "The variable " . ($stmt->var->name) . " became tainted.\n";
		  }
	       }

	       // Add the taint sets of the parents.
	       foreach($current_node->parents as $parent) {
	       		
			if ($tainted_variables_map->contains($parent)) {

			   $tainted_variables_map[$current_node]->addAll($tainted_variables_map[$parent]);
			}
	       }

	       $changed = $initial_tainted_size != $tainted_variables_map[$current_node]->count();

	       print "Finished processing node: \n";
	       $current_node->printCFGNode();

	       $tainted_variables_map[$current_node]->printTaintedVariables();
	       print "\n";

	       // Add the successors of the current node to the queue, if the tainted set has changed or the successor hasn't been visited.

	       foreach ($current_node->successors as $successor) {

	       	       if ($changed || !$tainted_variables_map->contains($successor)) {

			      $q->enqueue($successor);
		       }
	       }
	}

	print "==============================\n";
	print "The tainted variables at the exit node are:\n";
	$tainted_variables_map[$main_cfg->exit]->printTaintedVariables();
	print "\n";
	print "==============================\n";

}

?>