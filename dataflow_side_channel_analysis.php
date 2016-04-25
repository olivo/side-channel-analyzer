<?php

include_once(dirname(__FILE__) . '/TaintPHP/CallGraph/CallGraph.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFGNode.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFGNodeCond.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFGNodeStmt.php');
include_once(dirname(__FILE__) . '/TaintPHP/CFG/StmtProcessing.php');
include_once(dirname(__FILE__) . '/TaintPHP/PHP-Parser-master/lib/bootstrap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/CFGTaintMap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/TaintMap.php');
include_once(dirname(__FILE__) . '/TaintPHP/TaintAnalysis/TaintAnalysis.php');

// Performs side channel analysis across the entire application.
function sideChannelAnalysis($taintMap, $callGraph, $cfgInfoMap, $functionSignatures){

	 // Create a queue of call graph nodes of the functions to analyze.
	 $callGraphNodeQueue = new SplQueue();

	 // Create a set of call graph nodes currently in the queue,
	 // to prevent nodes from being added multiple times.
	 $callGraphNodeSet = new SplObjectStorage();

	 // Initially, add all the call graph leaves.
	 foreach($callGraph->getLeaves() as $callGraphNode) {
	     $callGraphNodeQueue->enqueue($callGraphNode);
	     $callGraphNodeSet->attach($callGraphNode);
	 }

	 // Process the nodes while the queue is not empty.
	 while(!$callGraphNodeQueue->isEmpty()) {

	     $callGraphNode = $callGraphNodeQueue->dequeue();

	     $signature = $callGraphNode->getFunctionRepresentation();
	     $fileName = $signature->getFileName();

	     // Process the CFG of a function if it's user defined.
	     if($fileName && isset($cfgInfoMap[$fileName])) {
	         $fileCFGInfo = $cfgInfoMap[$fileName];
	         $cfg = $fileCFGInfo->getCFG($signature);
		 $cfgTaintMap = $taintMap->get($signature->toString());
		 print "Starting side channel analysis on function with signature: " .
                       ($signature->toString()) . "\n";
		 cfg_dataflow_side_channel_detection($cfg, $cfgTaintMap);
		 print "Finished side channel analysis on function with signature: " .
                       ($signature->toString()) . "\n";
	     }

	     // Add the predecessors in the call graph, if they're not already
	     // present in the queue.
	     foreach($callGraphNode->getPredecessors() as $predecessor) {
	            if(!$callGraphNodeSet->contains($predecessor)) {
		    	    
		        $callGraphNodeSet->attach($predecessor);
			$callGraphNodeQueue->enqueue($predecessor);	        
		    }
	     }
	 }
}

// TODO: Hangs on openlinic/layout/admin.php

// Perform side-channel detection on the main and function CFGs.
function dataflow_side_channel_detection($fileCFGInfo, $fileTaintMaps) {

	 $mainCFG = $fileCFGInfo->getMainCFG();
	 $functionCFGs = $fileCFGInfo->getFunctionCFGs();

	 cfg_dataflow_side_channel_detection($mainCFG, $fileTaintMaps->getMainTaintMap());

	 foreach ($fileTaintMaps->getFunctionTaintMaps() as $funcName => $funcTaintMap) {

	     cfg_dataflow_side_channel_detection($functionCFGs[$funcName], $funcTaintMap);
	 }
}	 	 

// Performs a side-channel detection based on a dataflow analysis.
// The algorithm looks for imbalances in the number of database and loop 
// operations between two branches that depend on a secret.

function cfg_dataflow_side_channel_detection($cfg, $taint_maps) {	 	 

	 // WARNING: Imposing a bound to avoid infinite loop bugs.
	 // Use only for testing.
	 $bound = 10000;
	 $steps = 0;

	 $user_tainted_map = $taint_maps->getUserTaintMap();
	 $secret_tainted_map = $taint_maps->getSecretTaintMap();

	 print "Starting Dataflow Side Channel Detection.\n";
	 // Map that contains the number of loop operations from each node in the CFG.
	 $num_operations_map = new SplObjectStorage();
	 
	 // Backwards dataflow analysis for counting imbalance of operations at conditional nodes.
	 $exit_node = $cfg->exit;
	 $q = new SplQueue();
	 $q->enqueue($exit_node);

	 while (count($q) && $steps < $bound) {

	       $current_node = $q->dequeue();

	       $steps++;

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
	       // Arbitrarilly, propagate the maximum resource usage for the node.

	       // TODO: Keep the set of seen values, rather than the first successor value,
	       // for soundness purposes.
	       
	       $new_counter_value = 0;
	       if (CFGNode::isCFGNodeCond($current_node) && count($successor_array) > 1) {

	          if (isSecretTaintedCFGNodeCond($current_node, $secret_tainted_map[$current_node])) {
	             
		     print "ERROR: Side-channel vulnerability found at node: \n"; 
		     $current_node->printCFGNode();

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
		}
		else {

		  // Increment one to the successor counter only if current node
		  // is a loop header, and put the current value
		  // in the map.

		  $current_increment = 0;

		  if (CFGNode::isCFGNodeLoopHeader($current_node)) {
		     
		     $current_increment = 1;
		  }

		  $new_counter_value = (count($successor_array) ? array_keys($successor_array)[0] : 0) + $current_increment;
		   
		}

		print "Finished processing node: ";
		$current_node->printCFGNode();

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