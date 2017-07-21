<?php
include_once(dirname(__FILE__) . '/TaintPHP/CFG/CFG.php');

function computeConstraintMap($startNode) {
	 
	 $nodeQueue = new SplQueue();
	 $nodeConstraintsMap = new SplObjectStorage();

	 $nodeQueue->enqueue($startNode);

	 while(count($nodeQueue)) {
	 	$currentNode = $nodeQueue->dequeue();

		$update = False;

		// TODO: Keep only conditional constraints.
		if (!$nodeConstraintsMap->contains($currentNode)) {

		   $constraintSet = new SplObjectStorage();
		   $constraintsSet->attach($currentNode);
		   $nodeConstraintsMap[$currentNode] = $constraintsSet; 
		   $update = True;
		}
		else {
		   
		   if (!$nodeConstraintsMap[$currentNode]->contains($currentNode)) {
		      
		      $nodeConstraintsMap[$currentNode]->enqueue($currentNode);
		      $update = True;
		   }
		}

		if ($update) {
		   foreach($currentNode->parents as $parent) {
		      $nodeQueue->enqueue($parent);
		   }
		}
	 }

	 return $nodeConstraintsMap;
}

function processAssignment($currentNode, $constraintsMap) {
}
?>