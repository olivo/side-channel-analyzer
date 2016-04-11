/**
 * deleted_patient_tbl.sql
 *
 * Creation of deleted_patient_tbl structure
 *
 * Licensed under the GNU GPL. For full terms see the file LICENSE.
 *
 * @package   OpenClinic
 * @copyright 2002-2013 jact
 * @license   http://www.gnu.org/copyleft/gpl.html GNU GPL
 * @version   CVS: $Id: deleted_patient_tbl.sql,v 1.12 2013/01/16 19:02:26 jact Exp $
 * @author    jact <jachavar@gmail.com>
 * @since     0.2
 */

CREATE TABLE deleted_patient_tbl (
  id_patient INT UNSIGNED NOT NULL,
  nif VARCHAR(20) NULL,
  first_name VARCHAR(25) NOT NULL,
  surname1 VARCHAR(30) NOT NULL,
  surname2 VARCHAR(30) NULL DEFAULT '',
  address TEXT NULL,
  phone_contact TEXT NULL,
  sex ENUM('V','H') NOT NULL DEFAULT 'V',
  race VARCHAR(25) NULL, /* raza: blanca, amarilla, cobriza, negra */
  birth_date DATE NULL, /* fecha de nacimiento */
  birth_place VARCHAR(40) NULL, /* lugar de nacimiento */
  decease_date DATE NULL, /* fecha de defunci�n */
  nts VARCHAR(30) NULL, /* n�mero de tarjeta sanitaria */
  nss VARCHAR(30) NULL, /* n�mero de la seguridad social */
  family_situation TEXT NULL, /* situaci�n familiar */
  labour_situation TEXT NULL, /* situaci�n laboral */
  education TEXT NULL, /* estudios */
  insurance_company VARCHAR(30) NULL, /* entidad aseguradora */
  id_member INT UNSIGNED NULL, /* clave del m�dico al que pertenece por cupo */
  collegiate_number VARCHAR(20) NULL, /* numero de colegiado (del m�dico al que pertenece por cupo) */
  birth_growth TEXT NULL, /* nacimiento y crecimiento (desarrollo) */
  growth_sexuality TEXT NULL, /* desarrollo y vida sexual */
  feed TEXT NULL, /* alimentaci�n */
  habits TEXT NULL, /* h�bitos */
  peristaltic_conditions TEXT NULL, /* condiciones perist�ticas */
  psychological TEXT NULL, /* psicol�gicos */
  children_complaint TEXT NULL, /* enfermedades de la infancia */
  venereal_disease TEXT NULL, /* enfermedades de transmisi�n sexual */
  accident_surgical_operation TEXT NULL, /* accidentes e intervenciones quir�rgicas */
  medicinal_intolerance TEXT NULL, /* intolerancia medicamentosa */
  mental_illness TEXT NULL, /* enfermedades mentales y neur�ticas */
  parents_status_health TEXT NULL, /* estado de salud de los padres */
  brothers_status_health TEXT NULL, /* estado de salud de los hermanos */
  spouse_childs_status_health TEXT NULL, /* estado de salud del c�nyuge e hijos */
  family_illness TEXT NULL, /* enfermedades acumuladas en la familia */
  create_date DATETIME NOT NULL,
  id_user INT UNSIGNED NOT NULL,
  login VARCHAR(20) NOT NULL
) ENGINE=MyISAM;
