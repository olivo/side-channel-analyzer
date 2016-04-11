/**
 * deleted_problem_tbl.sql
 *
 * Creation of deleted_problem_tbl structure
 *
 * Licensed under the GNU GPL. For full terms see the file LICENSE.
 *
 * @package   OpenClinic
 * @copyright 2002-2013 jact
 * @license   http://www.gnu.org/copyleft/gpl.html GNU GPL
 * @version   CVS: $Id: deleted_problem_tbl.sql,v 1.10 2013/01/07 18:18:02 jact Exp $
 * @author    jact <jachavar@gmail.com>
 * @since     0.2
 */

CREATE TABLE deleted_problem_tbl (
  id_problem INT UNSIGNED NOT NULL,
  last_update_date DATE NOT NULL, /* fecha de �ltima actualizaci�n */
  id_patient INT UNSIGNED NOT NULL,
  id_member INT UNSIGNED NULL, /* clave del m�dico que atiende el problema */
  collegiate_number VARCHAR(20) NULL, /* numero de colegiado (del m�dico al que pertenece por cupo) */
  order_number TINYINT UNSIGNED NOT NULL, /* n�mero de orden relativo a cada paciente */
  opening_date DATE NOT NULL, /* fecha de apertura */
  closing_date DATE NULL, /* fecha de cierre */
  meeting_place VARCHAR(40) NULL, /* lugar de encuentro */
  wording TEXT NOT NULL, /* enunciado del problema */
  subjective TEXT NULL, /* subjetivo */
  objective TEXT NULL, /* objetivo */
  appreciation TEXT NULL, /* valoraci�n */
  action_plan TEXT NULL, /* plan de actuaci�n */
  prescription TEXT NULL, /* prescripci�n (por prescripci�n facultativa, on doctor's orders) */
  create_date DATETIME NOT NULL,
  id_user INT UNSIGNED NOT NULL,
  login VARCHAR(20) NOT NULL
) ENGINE=MyISAM;
