<?php
include_once "CFGNode.php";

// Class that corresponds to an individual statement CFG node (assignment, method calls, etc.).

class CFGNodeStmt extends CFGNode {

// The statement contained in the node.
public $stmt = NULL;

// Determines whether the successor of this node is reached by a back edge.
public $has_backedge = FALSE;

public function __construct() {

       parent::__construct();

       $this->stmt = NULL;

       $this->back_edge = FALSE;
}

// Printout function.
public function printCFGNode() {

       print "Statement Node.\n";
}
	
}
?>