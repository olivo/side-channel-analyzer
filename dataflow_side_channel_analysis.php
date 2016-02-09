<?php

include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFGNode.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFGNodeCond.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFGNodeStmt.php');
include_once(dirname(__FILE__) . '/TaintPHP/PHP-Parser-master/lib/bootstrap.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/StmtProcessing.php');


// Performs a side-channel detection based on a dataflow analysis.
// The algorithm looks for imbalances in the number of database and loop 
// operations between two branches that depend on a secret.

function dataflow_side_channel_detection($main_cfg, $function_cfgs, $function_signatures, $user_taint_map, $secret_tainted_map) {

	 print "Starting Dataflow Side Channel Detection.\n";
	 // Map that contains the number of database and loop operations from each node in the CFG.
	 $num_operations_map = new SplObjectStorage();
	 
	 // Backwards dataflow analysis for counting imbalance of operations at conditional nodes.
	 $exit_node = $main_cfg->exit;
	 $q = new SplQueue();
	 $q->enqueue($exit_node);

	 while (count($q)) {
	       
	       $current_node = $q->dequeue();

	       print "Current Node:\n";
	       $current_node->printCFGNode();
	       
	       // Obtain the counts for all the successors.
	       $successor_array = array();
	       
	       foreach ($current_node->successors as $successor) {

	       	   if ($num_operations_map->contains($successor)) {
		      
		      $successor_array[$num_operations_map[$successor]] = 1;
		   }
	       }

	       // If there are multiple count values for the successors
	       // of the current node, report side-channel vulnerability.
	       // Arbitrarilly, keep the first of these values for the current node.

	       // TODO: Keep the set of seen values, rather than the first successor value,
	       // for soundness purposes.
	       
	       $new_counter_value = 0;
	       if (count($successor_array) > 1) {

	          // TODO: Figure out how to print conditional nodes properly.					   
		  print "ERROR: Side-channel vulnerability found at node: \n"; 
		  //printStmts(array($current_node->stmt));
		  print "The successors counters are:\n";
		  $successor_keys = array_keys($successor_array);
		  foreach ($successor_keys as $counter) {
		  
		  	  print $counter . "\n";
		  }

		  // TODO: Currently setting the counter of a vulnerable
		  // node to the maximum.
		  // We should find a better way to propagate an error.
		  $new_counter_value = max($successor_array);

		  // TODO: Find something better than returning after the first error.
		  return;

		}
		else {

		  // Increment one to the successor counter only if current node
		  // is a database operation or a loop header, and put the current value
		  // in the map.

		  $current_increment = 0;
		  // TODO: Only counting loop headers. Need to fix this to count database operations.
		  if (CFGNode::isCFGNodeLoopHeader($current_node)) {
		     
		     $current_increment = 1;
		  }

		  $new_counter_value = (count($successor_array) ? array_keys($successor_array)[0] : 0) + $current_increment;
		   
		}

		print "Finished processing node: ";
		if (CFGNode::isCFGNodeStmt($current_node)) { 
		   
		   if ($current_node->stmt) {
		     printStmts(array($current_node->stmt));
		   }
		   else {
		     print "Dummy node.\n";
		   }
		}
		else {
		   print "Cond Node.\n";
		}

		// Update the counter for the current node.
		// If the value has changed, add the parents of the current node to the queue.
		if (!$num_operations_map->contains($current_node) || $num_operations_map[$current_node] != $new_counter_value) {

		   print "Updated value for node:\n";
		   $current_node->printCFGNode();

		   $num_operations_map[$current_node] = $new_counter_value;
		   // Add the parents of the current node to the queue.
		   foreach ($current_node->parents as $parent) {
		   
			$q->enqueue($parent);
		   }
		}
	 }
 }


?>