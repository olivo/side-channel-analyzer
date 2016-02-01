<?php

include_once "CFGNode.php";
include_once "CFGNodeCond.php";
include_once "CFGNodeStmt.php";
include_once "PHP-Parser-master/lib/bootstrap.php";
include_once "StmtProcessing.php";
include_once "TaintedVariables.php";

// TODO: Change hardwired notions of taint for a specific application.
// Checks whether an expression is tainted, by checking whether a parameter is a tainted variable or a user/secret input. The $user_taint parameter is True when checking for user taint, and 
// false when checking for secret taint.
function isTainted($expr, $tainted_variables, $user_taint) {

       print "Analyzing expression for taint.\n";
       print "The class is " . get_class($expr) . "\n";

       if ($expr == null) {
       
	return false;
       }

       // For now, checking that the expression is either a function call of 'postGetSession' or a variable already in the tainted set.

       if ($expr instanceof PhpParser\Node\Expr\StaticCall || $expr instanceof PhpParser\Node\Expr\FuncCall || $expr instanceof PhpParser\Node\Expr\MethodCall) {

       	  print "Analyzing static call, function call or method call for taint\n";       	  
	  $function_name = $expr->name;

	  // The expression is tainted if it invokes a basic user input extraction function.
	  if ($user_taint && (strcmp($function_name, 'postGetSessionInt') == 0 || strcmp($function_name, 'postGetSessionString') == 0)) {

	     return true;
	  }
	  else if (!$user_taint && strcmp($function_name, 'search') == 0) {

	     // The expression is tainted if it invokes the secret-tainting function in openclinic.
	     return true;
	  }

	  // The expression is tainted if one of the arguments is tainted.
	  foreach ($expr->args as $arg) {
	  	  
		  if (isTainted($arg->value, $tainted_variables, $user_taint)) {

		  	return true;
		  }
	  }
	  
	  // The expression is tainted if it is a method call over a tainted expression.
	  if ($expr instanceof PhpParser\Node\Expr\MethodCall && isTainted($expr->var, $tainted_variables, $user_taint)) {

	     return true;
	  }
       }
       else if ($expr instanceof PhpParser\Node\Expr\Variable) {

       	  print "Analyzing variable for taint : " . ($expr->name) . "\n";
       
	  return $tainted_variables->contains($expr->name);
       }
       else if ($expr instanceof PhpParser\Node\Expr\BinaryOp) {

       	  print "Analyzing binary op for taint.\n";
	  return isTainted($expr->left, $tainted_variables, $user_taint) || 
	  	 isTainted($expr->right, $tainted_variables, $user_taint);
       }
       else if ($expr instanceof PhpParser\Node\Expr\ArrayDimFetch) {

       	  print "Analyzing array fetch expression for taint.\n";
	  return isTainted($expr->var, $tainted_variables, $user_taint) || isTainted($expr->dim, $tainted_variables, $user_taint);
       }
       
       return false;
}

// Processes taint for a CFG node.
function processTaint($current_node, $user_tainted_variables_map, $secret_tainted_variables_map) {

	       // Check if the current node is a statement node with a 
	       // non-null statement.
	       if (CFGNode::isCFGNodeStmt($current_node) && $current_node->stmt) {

	       	  $stmt = $current_node->stmt;
	       	  // Check to see if the statement is an assigment,
		  // and the right hand side is tainted.
		  if (($stmt instanceof PhpParser\Node\Expr\Assign) || ($stmt instanceof PhpParser\Node\Expr\AssignOp)) {

		      // Accounting for simple LHS variables and array indexing.
		      $lhs = $stmt->var;
		      
		      if ($lhs instanceof PhpParser\Node\Expr\Variable) {
		      	 
			 $lhs_var = $lhs->name;
		      } 
		      else if ($lhs instanceof PhpParser\Node\Expr\ArrayDimFetch) {

		      	 $lhs_var = $lhs->var->name;
		      } else {

		      	 print "ERROR: Unrecognized LHS type of an assignment while performing taint analysis.\n";
		      } 
		     
		      if (!$user_tainted_variables_map[$current_node]->contains($lhs_var)
		          && isTainted($stmt->expr, $user_tainted_variables_map[$current_node], True)) {

		      	 $user_tainted_variables_map[$current_node]->attach($lhs_var);
		     	 print "The variable " . ($lhs_var) . " became user-tainted.\n";
		      }

		      if (!$secret_tainted_variables_map[$current_node]->contains($lhs_var)
		          && isTainted($stmt->expr, $secret_tainted_variables_map[$current_node], False)) {

		      	 $secret_tainted_variables_map[$current_node]->attach($lhs_var);
		     	 print "The variable " . ($lhs_var) . " became secret-tainted.\n";
		      }
		  }
		  // or a method call with a tainting method.
		  else if ($stmt instanceof PhpParser\Node\Expr\MethodCall) {

		      if (!$user_tainted_variables_map[$current_node]->contains($stmt->var->name)
		          && isTainted($stmt, $user_tainted_variables_map[$current_node], True)) {

		     	  $user_tainted_variables_map[$current_node]->attach($stmt->var->name);
		     	  print "The variable " . ($stmt->var->name) . " became user-tainted.\n";
		      }

		      if (!$secret_tainted_variables_map[$current_node]->contains($stmt->var->name)
		          && isTainted($stmt, $secret_tainted_variables_map[$current_node], False)) {

		     	  $secret_tainted_variables_map[$current_node]->attach($stmt->var->name);
		     	  print "The variable " . ($stmt->var->name) . " became secret-tainted.\n";
		      }
		  }
	       }
	       // Check if a conditional node is tainted, and issue a warning.
	       else if (CFGNode::isCFGNodeCond($current_node) && $current_node->expr) {

	       	    // If the conditional contains an assignment, propagate its taint
		    // besides checking for taint in the conditional.
		    if ($current_node->expr instanceof PhpParser\Node\Expr\Assign) {

		       if (!$user_tainted_variables_map[$current_node]->contains($current_node->expr->var->name)
		           && isTainted($current_node->expr->expr, $user_tainted_variables_map[$current_node], True)) {
		       	  
			  print "The variable " . ($current_node->expr->var->name) . "became user tainted.\n";
			  $user_tainted_variables_map[$current_node]->attach($current_node->expr->var->name);
	       	    	  print "WARNING: Conditional node is user-tainted:\n";
		       }

		       if (!$secret_tainted_variables_map[$current_node]->contains($current_node->expr->var->name)
		           && isTainted($current_node->expr->expr, $secret_tainted_variables_map[$current_node], False)) {
		       	  
			  print "The variable " . ($current_node->expr->var->name) . "became secret tainted.\n";
			  $secret_tainted_variables_map[$current_node]->attach($current_node->expr->var->name);
	       	    	  print "WARNING: Conditional node is secret-tainted:\n";
		       }
		    }
		    else {
	       	        
			if (isTainted($current_node->expr, $secret_tainted_variables_map[$current_node], False)) {

	       	    	   print "WARNING: Conditional node is secret-tainted:\n";
			}

			if (isTainted($current_node->expr, $user_tainted_variables_map[$current_node], True)) {

	       	    	   print "WARNING: Conditional node is user-tainted:\n";
			}
		    }
	       }
	       // Check if a loop header is secret-tainted, and issue a warning.
	       else if (CFGNode::isCFGNodeLoopHeader($current_node) && $current_node->expr) {

	            print "Analyzing loop header.\n";
	            // The conditional covers the case when the condition is a boolean expression or an 
		    // assignment that propagates taint.
	       	    if ($current_node->isWhileLoop()) {

		       if (isTainted($current_node->expr->cond, $user_tainted_variables_map[$current_node], True)
			    || ($current_node->expr->cond instanceof PhpParser\Node\Expr\Assign 
			        && isTainted($current_node->expr->cond->expr, $user_tainted_variables_map[$current_node], True))) {
		       
				print "While Loop is secret-tainted.\n";
			}
	       }
}

// Performs a flow-sensitive forward taint analysis.
function taint_analysis($main_cfg, $function_cfgs, $function_signatures) {

	 print "Starting Taint Analysis.\n";

	 // Map that contains the set of tainted variables 
	 // per CFG node.
	 $user_tainted_variables_map = new SplObjectStorage();
	 $secret_tainted_variables_map = new SplObjectStorage();

	 // Forward flow-sensitive taint-analysis.
	 $entry_node = $main_cfg->entry;
	 $q = new SplQueue();
	 $q->enqueue($entry_node);

	 while (count($q)) {
	       
	       $current_node = $q->dequeue();

	       if (!$user_tainted_variables_map->contains($current_node)) {

	       	  $user_tainted_variables_map[$current_node] = new TaintedVariables();
	       }

	       if (!$secret_tainted_variables_map->contains($current_node)) {

	       	  $secret_tainted_variables_map[$current_node] = new TaintedVariables();
	       }

	       print "Started processing node: \n";
	       $current_node->printCFGNode();

	       $initial_user_tainted_size = $user_tainted_variables_map[$current_node]->count();
	       $initial_secret_tainted_size = $secret_tainted_variables_map[$current_node]->count();

	       // Add the taint sets of the parents.
	       foreach($current_node->parents as $parent) {
	       		
			if ($user_tainted_variables_map->contains($parent)) {

			   $user_tainted_variables_map[$current_node]->addAll($user_tainted_variables_map[$parent]);
			}

			if ($secret_tainted_variables_map->contains($parent)) {

			   $secret_tainted_variables_map[$current_node]->addAll($secret_tainted_variables_map[$parent]);
			}
	       }

	       // Process taint for the current node.
	       processTaint($current_node, $user_tainted_variables_map, $secret_tainted_variables_map);

	       $changed = $initial_user_tainted_size != $user_tainted_variables_map[$current_node]->count() 
	       		  || $initial_secret_tainted_size != $secret_tainted_variables_map[$current_node]->count() ;

	       print "Finished processing node: \n";
	       $current_node->printCFGNode();

	       print "User tainted variables:\n";
	       $user_tainted_variables_map[$current_node]->printTaintedVariables();
	       print "Secret tainted variables:\n";
	       $secret_tainted_variables_map[$current_node]->printTaintedVariables();
	       print "\n";

	       // Add the successors of the current node to the queue, if the tainted set has changed or the successor hasn't been visited.

	       foreach ($current_node->successors as $successor) {

	       	       if ($changed || !$user_tainted_variables_map->contains($successor) 
		                    || !$secret_tainted_variables_map->contains($successor)) {

			      $q->enqueue($successor);
		       }
	       }
	}

	print "==============================\n";
	print "The user tainted variables at the exit node are:\n";
	$user_tainted_variables_map[$main_cfg->exit]->printTaintedVariables();
	print "\n";
	print "==============================\n";
	print "==============================\n";
	print "The secret tainted variables at the exit node are:\n";
	$secret_tainted_variables_map[$main_cfg->exit]->printTaintedVariables();
	print "\n";
	print "==============================\n";

}

?>