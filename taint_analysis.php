<?php

include_once "CFGNode.php";
include_once "CFGNodeCond.php";
include_once "CFGNodeStmt.php";
include_once "PHP-Parser-master/lib/bootstrap.php";
include_once "StmtProcessing.php";

// TODO: Change hardwired notions of taint for a specific application.
// Checks whether an expression is tainted, by checking whether a parameter is a tainted variable or a user input.
function isTainted($expr, $taint_variables) {

       // For now, checking that the expression is either a function call of 'postGetSession' or a variable already in the tainted set.

       if ($expr instanceof PhpParser\Node\Expr\StaticCall) {
       	  
	  $function_name = $expr->name;
	  return strcmp($function_name, 'postGetSessionInt') == 0 || strcmp($function_name, 'postGetSessionString') == 0 ;
       }
       else if ($expr instanceof PhpParser\Node\Expr\Variable) {

	  $variable_name = $expr->name;
       
	  return $tainted_variables->contains($variable_name);
       }
       
       return false;
}

// Performs a flow-insensitive forward taint analysis.
function taint_analysis($main_cfg, $function_cfgs, $function_signatures) {

	 print "Starting Taint Analysis.\n";

	 // Set that contains tainted variables.
	 $taint_variables = new SplObjectStorage();

	 // Set of visited nodes.
	 $visited_nodes = new SplObjectStorage();
	 
	 // Forward flow-insensitive taint-analysis.
	 $entry_node = $main_cfg->entry;
	 $q = new SplQueue();
	 $q->enqueue($entry_node);

	 while (count($q)) {
	       
	       $current_node = $q->dequeue();
	       $visited_nodes->attach($current_node);

	       print "Started processing node: \n";
	       CFGNode::printCFGNode($current_node);

	       // Check if the current node is a statement node with a 
	       // non-null statement.
	       if (CFGNode::isCFGNodeStmt($current_node) && $current_node->stmt) {

	       	  $stmt = $current_node->stmt;
	       	  // Check to see if the statement is an assigment,
		  // and the right hand side is tainted.
		  if (($stmt instanceof PhpParser\Node\Expr\Assign) && isTainted($stmt->expr, $taint_variables)) {
		     
		     $taint_variables->attach($stmt->var);
		     print "The variable " . ($stmt->var->name) . " became tainted.\n";
		  }
	       }

	       print "Finished processing node: \n";
	       CFGNode::printCFGNode($current_node);

	       // Add the successors of the current node to the queue.
	       foreach ($current_node->successors as $successor) {

	       	       if (!$visited_nodes->contains($successor)) {
			   $q->enqueue($successor);
		       }
	       }
	}

	print "==============================\n";
	print "The tainted variables are:\n";
	foreach ($taint_variables as $tv) {

	    print $tv->name . " ";
	}
	print "\n";
	print "==============================\n";

}

?>