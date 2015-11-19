<?php

include_once "PHP-Parser-master/lib/bootstrap.php";
include_once "CFGNode.php";
include_once "StmtProcessing.php";
include_once "FunctionSignature.php";

// Class representing an entire CFG.
// It contains an entry CFG node, and an exit CFG node.
// The CFG can be traversed by going through the successors of 
// the CFG nodes until the exit node.

class CFG {

      // Entry node.
      public $entry = NULL;

      // Exit node.
      public $exit = NULL;

      function __construct() {

      	       $this->entry = new CFGNode();
	       $this->exit = new CFGNode();

      }

	
	// Construct the Control Flow Graph (CFG) from a 
	// sequence of statements.

	static function construct_cfg($stmts) {

	       $prettyPrinter = new PhpParser\PrettyPrinter\Standard;

	       // Creating an empty entry node.	
	       $cfg = new CFG();

	       $entry = new CFGNode();

	       $cfg->entry = $entry;
	       
	       $current_node = $entry;

	       foreach($stmts as $stmt)  {

	         print "Processing statement:\n";
	       	 printStmts(array($stmt));

		 // Assignment statement.
	       	 if($stmt instanceof PhpParser\Node\Expr\Assign) {
		 	  print "Found assignment statement\n";
			  $assign_node = CFG::processExprAssign($stmt);
			  $current_node->successors[] = $assign_node;
			  $current_node = $assign_node;
			  print "Constructed assignment node\n";
		  }
		 // Assignment with operation statement.
	       	 else if($stmt instanceof PhpParser\Node\Expr\AssignOp) {
		 	  print "Found assignment with operation statement\n";
			  $assign_op_node = CFG::processExprAssignOp($stmt);
			  $current_node->successors[] = $assign_op_node;
			  $current_node = $assign_op_node;
			  print "Constructed assignment with operation node\n";
		  }
		 //  Pre-decrement expression.
	       	 else if($stmt instanceof PhpParser\Node\Expr\PreDec) {
		 	  print "Found a pre-decrement expression\n";
			  $predec_node = CFG::processExprPreDec($stmt);
			  $current_node->successors[] = $predec_node;
			  $current_node = $predec_node;
			  print "Constructed pre-decrement expression.\n";
		  }
		 // Unset statement.
	       	 else if($stmt instanceof PhpParser\Node\Stmt\Unset_) {
		 	  print "Found unset statement\n";
			  $unset_node = CFG::processStmtUnset($stmt);
			  $current_node->successors[] = $unset_node;
			  $current_node = $unset_node;
			  print "Constructed unset node\n";
		  }
		 // Global declaration statement.
	       	 else if($stmt instanceof PhpParser\Node\Stmt\Global_) {
		 	  print "Found global statement\n";
			  $global_node = CFG::processStmtGlobal($stmt);
			  $current_node->successors[] = $global_node;
			  $current_node = $global_node;
			  print "Constructed global node\n";
		  }
		 // Break statement.
	       	 else if($stmt instanceof PhpParser\Node\Stmt\Break_) {
		 	  print "Found break statement\n";
			  $break_node = CFG::processStmtBreak($stmt);
			  $current_node->successors[] = $break_node;
			  $current_node = $break_node;
			  print "Constructed break node\n";
		  }
		 // Return statement.
	       	 else if($stmt instanceof PhpParser\Node\Stmt\Return_) {
		 	  print "Found return statement\n";
			  $return_node = CFG::processStmtReturn($stmt);
			  $current_node->successors[] = $return_node;
			  $current_node = $return_node;
			  print "Constructed return node\n";
		  }
		  // If statement.
		  else if($stmt instanceof PhpParser\Node\Stmt\If_) {
		          print "Found conditional statement\n";
			  $if_nodes = CFG::processStmtIf($stmt);

			  // Connect the current node with the 
			  // conditional node of the if.
			  $current_node->successors[]=$if_nodes[0];

			  // Make the current node, the node that 
			  // joins the branches of the if.
			  $current_node = $if_nodes[1];
			  
		       	  print "Constructed conditional node\n";

		  // Method call statement.
		  } else if($stmt instanceof PhpParser\Node\Expr\MethodCall) {

		 	  print "Found method call statement\n";
			  $method_call_node = CFG::processExprMethodCall($stmt);
			  $current_node->successors[] = $method_call_node;
			  $current_node = $method_call_node;
			  print "Constructed method call node\n";


		  

		  // Function call statement.
		  } else if($stmt instanceof PhpParser\Node\Expr\FuncCall) {

		 	  print "Found function call statement\n";
			  $function_call_node = CFG::processExprFuncCall($stmt);
			  $current_node->successors[] = $function_call_node;
			  $current_node = $function_call_node;
			  print "Constructed function call node\n";


		  
		  // Static function call statement.
		  } else if($stmt instanceof PhpParser\Node\Expr\StaticCall) {

		 	  print "Found static call statement\n";
			  $static_call_node = CFG::processExprStaticCall($stmt);
			  $current_node->successors[] = $static_call_node;
			  $current_node = $static_call_node;
			  print "Constructed static call node\n";


		  

		  // Foreach statement.
		  } else if($stmt instanceof PhpParser\Node\Stmt\Foreach_) {

		 	  print "Found Foreach statement\n";
			  // Returns a pair with the loop header 
			  // and a dummy exit node that follows the
			  // loop.
			  $foreach_nodes = CFG::processStmtForeach($stmt);

			  // Connect the current node to the loop header.
			  $current_node->successors[] = $foreach_nodes[0];

			  // Make the dummy exit node of the loop
			  // the current node.
			  $current_node = $foreach_nodes[1];

			  print "Constructed Foreach node\n";

		  // For statement.
		  } else if($stmt instanceof PhpParser\Node\Stmt\For_) {

		 	  print "Found For statement\n";
			  // Returns a pair with the loop header 
			  // and a dummy exit node that follows the
			  // loop.
			  $for_nodes = CFG::processStmtFor($stmt);

			  // Connect the current node to the loop header.
			  $current_node->successors[] = $for_nodes[0];

			  // Make the dummy exit node of the loop
			  // the current node.
			  $current_node = $for_nodes[1];

			  print "Constructed For node\n";

   }	       		      

		  else {	       		      
		       	  print "WARNING: Couldn't construct CFG node.\n";
		  	  print "The statement has type ".($stmt->getType())."\n";

	          	  print "Has keys\n";

		  	  foreach($stmt as $key => $value) {
			  		print "Key=".($key)."\n";
		   	   }

		  }




	        }
	 
	// Create a dummy exit node, and make a pointer
	// from the last processed node to the exit node.
	$cfg->exit = new CFGNode();
	$current_node->successors[] = $cfg->exit;

	return $cfg;
					
}	

// Constructs a node for an assignment expression.
static function processExprAssign($exprAssign) {

	// $exprAssign has keys 'var' and 'expr'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $exprAssign;

	return $cfg_node;
}

// Constructs a node for an assignment with operation expression.
static function processExprAssignOp($exprAssignOp) {

	// $exprAssign has keys 'var' and 'expr'.
	// It can be extended by classes Div, Minus, Plus, etc.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $exprAssignOp;

	return $cfg_node;
}

// Constructs a node for a pre-decrement expression.
static function processExprPreDec($exprPreDec) {

	// $exprPreDec has key 'var'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $exprPreDec;

	return $cfg_node;
}

// Constructs a node for an assignment expression.
static function processStmtUnset($stmtUnset) {

	// $stmtUnset has keys 'vars'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $stmtUnset;

	return $cfg_node;
}

// Constructs a node for a global declaration statement.
static function processStmtGlobal($stmtGlobal) {

	// $stmtGlobal has keys 'vars'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $stmtGlobal;

	return $cfg_node;
}

// Constructs a node for a break statement.
static function processStmtBreak($stmtBreak) {

	// $stmtBreak has keys 'num'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $stmtBreak;

	return $cfg_node;
}

// Constructs a node for a return statement.
static function processStmtReturn($stmtReturn) {

	// $stmt has key 'expr'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $stmtReturn;

	return $cfg_node;
}


// WARNING: Doesn't handle interprocedural case yet.
// Constructs a node for a method call expression.
static function processExprMethodCall($exprMethodCall) {

	// $exprMethodCall has keys 'var', 'name' and 'args'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $exprMethodCall;

	return $cfg_node;
}

// WARNING: Doesn't handle interprocedural case yet.
// Constructs a node for a function call expression.
static function processExprFuncCall($exprFuncCall) {

	// exprFuncCall has keys 'name' and 'args'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $exprFuncCall;

	return $cfg_node;
}

// WARNING: Doesn't handle interprocedural case yet.
// Constructs a node for a static call expression.
static function processExprStaticCall($exprStaticCall) {

	// exprFuncCall has keys 'class', 'name' and 'args'.

	$cfg_node = new CFGNode();
	$cfg_node->stmt = $exprStaticCall;

	return $cfg_node;
}

// Constructs a node for an if statement.
// 1) Creates a node for each condition, and constructs an array  called condition array.
// 2) Creates a CFG for each conditioned block, and constructs an array called body array.
// 3) It creates a dummy exit node that all the statement blocks will converge into.
// 4) Links all the exits of the body CFGs to the exit dummy node.
// 5) Links each condition node to its corresponding body array.
// 6) Links each condition node to its next condition node.
// 7) Links the last condition node to the next body CFG if it exists, or the dummy exit node otherwise.
// It returns the first condition node and dummy exit nodes.

static function processStmtIf($stmtIf) {

	// stmtIf has keys 'cond', 'stmts', 'elseifs', and 'else'.

	// Array of CFG nodes representing the conditions.
	$cond_nodes = array();

	// Array of CFGs representing the bodies of each conditional branch.
	$body_nodes = array();

	// Create and add the top condition node.
	$cond_node = new CFGNode();
	$cond_node->stmt = $stmtIf->cond;
	$cond_node->is_cond = TRUE;
	$cond_nodes[] = $cond_node;
	
	// Create and add the true branch of the top condition node.
	$body_node = CFG::construct_cfg($stmtIf->stmts);
	$body_nodes[] = $body_node;

	// Create and add the condition nodes for the else if clauses.
	foreach($stmtIf->elseifs as $elseif) {

		$cond_node = new CFGNode();
		$cond_node->stmt = $elseif->cond;		
		$cond_node->is_cond = TRUE;
		$cond_nodes[] = $cond_node;

		$body_node = CFG::construct_cfg($elseif->stmts);
		$body_nodes[] = $body_node;
	
	 }

	 // Create and add the else body node if it exists
	 if($stmtIf->else) {
	 	$body_node = CFG::construct_cfg($stmtIf->else->stmts);
		$body_nodes[] = $body_node;
	  }

	// Create a dummy exit node from which the branch CFGs point to.
	$dummy_exit = new CFGNode();

	// Link the exits of all the body nodes to the dummy exit node.
	foreach($body_nodes as $body_node) 
			    $body_node->exit->successors[] = $dummy_exit;

	// Link the condition nodes to their corresponding entries of the body nodes.
	for($i=0;$i<count($cond_nodes);$i++)
		$cond_nodes[$i]->successors[] = $body_nodes[$i]->entry;

	// Link each condition node to the next condition node.
	for($i=0;$i<count($cond_nodes)-1;$i++)
		$cond_nodes[i]->successors[] = $cond_nodes[i+1];

	//Link the last condition node to the next body node if it exists or the dummy exit node.
	$last_index = count($cond_nodes)-1;
	if($last_index+1<count($body_nodes))
		$cond_nodes[$last_index]->successors[] = $body_nodes[$last_index+1]->entry;
	else
		$cond_nodes[$last_index]->successors[] = $dummy_exit;

	// Return the top condition node and the dummy exit node.
	return array($cond_nodes[0],$dummy_exit);

}

// Constructs a node for an include expression.
// WARNING: Not implemented;
static function processExprInclude($exprInclude) {

	// exprInclude has keys 'expr' and 'type'.
	print("WARNING:Expr Include not handled properly.\n");
	$cfg_node = new CFGNode();

	return $cfg_node;
}


// Constructs a node of a foreach loop.
// 1) Creates a CFG node for the loop condition that
// acts as the loop header.
// 2) Creates a CFG of the body of the loop.
// 3) Links the exit of the body CFG to the loop header CFG.
// 4) Creates an exit dummy node.
// 5) Links the condition node to the CFG of the body and the dummy
// exit node.
static function processStmtForeach($stmtForeach) {

	// $stmtForeach has keys 'expr', 'valueVar' and 'stmts', 
	// and optionally 'subNodes' which contains 'keyVar' and 'byRef'

	// Create the CFG node for the loop header.
	$header_node = new CFGNode();
	// The stmt in the header node is the pair ($collection,$var),
	// where the condition of the foreach is $
	$header_node->stmt = array($stmtForeach->expr,
				   $stmtForeach->valueVar);

	$header_node->is_cond = TRUE;
	$header_node->is_loop_header = TRUE;
	$header_node->loop_type = CFGNode::FOREACH_LOOP;

	// Create the dummy exit node.
	$dummy_exit = new CFGNode();

	// Create the CFG for the body of the loop.
	$body_cfg = CFG::construct_cfg($stmtForeach->stmts);

	// Link the exit of the body CFG to the loop header.
	$body_cfg->exit->successors[] = $header_node;

	// Assert that the edge from the exit of the body CFG to 
	// the loop header is a backedge
	$body_cfg->exit->has_backedge = TRUE;
	

	// Link the header node to the entry of the body CFG.
	$header_node->successors[] = $body_cfg->entry;

	// Link the header node to the dummy exit node.
	$header_node->successors[] = $dummy_exit;

	return array($header_node,$dummy_exit);
}

// Constructs a node of a for loop.
// 1) Creates a CFG node for the loop initialization, conditions 
// and loop increments that
// acts as the loop header.
// 2) Creates a CFG of the body of the loop.
// 3) Links the exit of the body CFG to the loop header CFG.
// 4) Creates an exit dummy node.
// 5) Links the condition node to the CFG of the body and the dummy
// exit node.
static function processStmtFor($stmtFor) {

	// $stmtFor has keys 'init', 'cond', 'loop' and 'stmts'.

	// Create the CFG node for the loop header.
	$header_node = new CFGNode();
	// The stmt in the header node is the triple ($init,$cond,$loop),
	// where 'init' is an array of initializations expressions,
	// 'cond' are loop conditions, and 'loop' are 
	// incrementing expressions.
 
	$header_node->stmt = array($stmtFor->init,
				   $stmtFor->cond,
				   $stmtFor->loop);

	//print "The init value is :".printStmts($stmtFor->init)."\n";
	//print "The cond value is :".printStmts($stmtFor->cond)."\n";
	//print "The loop value is :".printStmts($stmtFor->loop)."\n";

	$header_node->is_cond = TRUE;
	$header_node->is_loop_header = TRUE;
	$header_node->loop_type = CFGNode::FOR_LOOP;

	// Create the dummy exit node.
	$dummy_exit = new CFGNode();

	// Create the CFG for the body of the loop.
	$body_cfg = CFG::construct_cfg($stmtFor->stmts);

	// Link the exit of the body CFG to the loop header.
	$body_cfg->exit->successors[] = $header_node;

	// Assert that exit of the body CFG has a back edge to the header.
	$body_cfg->exit->has_backedge = TRUE;

	// Link the header node to the entry of the body CFG.
	$header_node->successors[] = $body_cfg->entry;

	// Link the header node to the dummy exit node.
	$header_node->successors[] = $dummy_exit;

	return array($header_node,$dummy_exit);
}

// Prints a CFG starting from the root node.
// WARNING: Only printing the true branches of the conditionals.
function print_cfg() {
	 
	 print "Starting to print CFG\n";
	 print "WARNING: Only printing the true branches of the conditionals\n";

	 // Skip the first node, because it's a dummy entry node.
	 $current_node=$this->entry;
	 $successor_list = $current_node->successors;
	 $current_node=$successor_list[0];
	 

	 do {

	 	if($current_node->stmt) {
			if(!$current_node->loop_type) {
			print "Statement in node.\n";
			printStmts(array($current_node->stmt));
			print "With type: ".($current_node->stmt->getType())."\n";
			print "It has ".(count($current_node->successors)) ." successors.\n";
			if($current_node->is_cond)
				print "It is a CONDITIONAL\n";

			/*
		       // If it's a back-edge stop.
		       if($current_node->has_backedge)
		       		       break;
		       */
		       $current_node = $current_node->successors[0];

    			 }
			 else {
			      if($current_node->loop_type==
				 CFGNode::FOREACH_LOOP) {
				  	 
			     	  // If it's a foreach node, print the 
			      	  // header especially, and continue on the
			      	  // false branch -- to avoid infinite
			      	  // loops.
				  print "It is a foreach\n";

			      	  printStmts(array($current_node->stmt[0],
					       $current_node->stmt[1]));

			          print "It is a CONDITIONAL\n";
			       }
			       else if($current_node->loop_type==
			       	    CFGNode::FOR_LOOP) {
				    // It is a for loop.
				    print "It is a for loop!\n";
				    
				    print "Initialization:\n";
				    printStmts($current_node->stmt[0]); 

				    print "Condition:\n";
				    printStmts($current_node->stmt[1]); 

				    print "Increment:\n";
				    printStmts($current_node->stmt[2]); 

				    print "It is CONDITIONAL\n";
				    }
			       else {
			       	  print "WARNING: Unhandled loop type in print_cfg()\n";
			       }
// WARNING: Generates an infinite loop.
			      $current_node=$current_node->successors[1];     	      //$current_node=$current_node->successors[0];     

			 }
		 }		 	      
		 else {
		       print "Skipping null node\n";
		       $current_node = $current_node->successors[0];
		 }
		 

	 } while(count($current_node->successors));
	

}


	// Obtain the function declarations from a list of statements,
	// and return the mapping from function names to their CFGs
	// , as well as the mapping from function names to function
	// signatures.
	static function process_function_definitions($stmts) {
	       
	       // Map from function names to CFG.
	       $cfgMap = array();

	       // Map from function name to function signature.
	       $signatureMap = array();

	       foreach($stmts as $stmt) 
	       		      if($stmt instanceof PhpParser\Node\Stmt\Function_)	{
			      $signature = new FunctionSignature($stmt->name,$stmt->params,$stmt->returnType);

			      $name = $stmt->name;

			      $cfg = CFG::construct_cfg($stmt->stmts);
			      $cfgMap[(string)$stmt->name] = $cfg;
			      $signatureMap[(string)$stmt->name] = $signature;
	       }


	       return array($cfgMap,$signatureMap);
	       	       
	 }

// Opens a file, constructs the CFGs of the inner functions and 
// the main function, and returns the mapping from function names
// to CFGs, main CFG, and the mapping from function signatures
// to CFGs.
static function construct_file_cfgs($filename) {

	print "Constructing CFGs for file ".($filename)."\n";
	 	
	$file = fopen($filename,"r");

	$parser = new PhpParser\Parser(new PhpParser\Lexer);

	$contents = fread($file,filesize($filename));

	$stmts=array();

	try {
		$stmts = $parser->parse($contents);	
	} catch(PhpParser\Error $e) {
	  	echo 'Parse Error: ',$e->getMessage();
	}


	echo "There are ".count($stmts)." statements.\n"; 	

	// Construct the CFGs for all the functions defined in 
	// the file.

	echo "Constructing the CFG map of functions.\n";
	$function_definitions = CFG::process_function_definitions($stmts);
	$function_cfgs = $function_definitions[0];
	$function_signatures = $function_definitions[1];


	echo "Finished construction of the CFG map of functions.\n";
	echo "Found ".(count($function_signatures))." inner functions.\n";
	echo "The function names are:\n";
	foreach($function_signatures as $name => $signature)
				     print $name."\n";

	// Construct the CFG of the main procedure of the file.
	echo "Constructing the main CFG.\n";
	$main_cfg = CFG::construct_cfg($stmts);
	echo "Finished construction of the main CFG.\n";

	echo "The main CFG is:\n";
	$main_cfg->print_cfg();

/*
	echo "The CFGs of the inner functions are:\n";
	     foreach($function_cfgs as $name => $inner_cfg) {
		print "The CFG of ".$name." is :\n";
		$inner_cfg->print_cfg();
	 }
*/

	fclose($file);
		 
	return array($main_cfg,$function_cfgs,$function_signatures);
}
	

}

?>