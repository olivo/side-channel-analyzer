<?php
include_once "CFGNode.php";

// Class that represents a loop header.
class CFGNodeLoopHeader extends CFGNode {

// Types of loops.
const FOR_LOOP = 0;
const WHILE_LOOP = 1;
const FOREACH_LOOP = 2;

// Loop statement associated with the header.
public $stmt = NULL;

// Type of loop.
public $loop_type = NULL;

public function __construct() {

       parent::__construct();	      	     

       $this->stmt = NULL;
       $this->loop_type = NULL;
}

// The loop entry node is the first successor.
public function getLoopEntry() {

       return $this->successors[0];
}

// The loop exit node is the second successor.
public function getLoopExit() {

       return $this->successors[1];
}



// Printout function.
public function printCFGNode() {

      print "Loop header node.\n";
}
	
}
?>