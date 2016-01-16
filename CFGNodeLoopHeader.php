<?php
include_once "CFGNode.php";

// Class that represents a loop header.
class CFGNodeLoopHeader extends CFGNode {

// Loop statement associated with the header.
public $stmt = NULL;

public function __construct() {

       parent::__construct();	      	     

       $this->stmt = NULL;
}

// The loop entry node is the first successor.
public function getLoopEntry() {

       return $this->successors[0];
}

// The loop exit node is the second successor.
public function getLoopExit() {

       return $this->successors[1];
}
	
}
?>