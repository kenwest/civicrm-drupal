<?php

/**
 * Project:     CiviCRM: Constituent Relationship Management for NP's
 * File:        civicrm_user.inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/**
 * Drupal module include file.
 *
 * @package CRM
 * @copyright CiviCRM LLC (c) 2004-2014
 * $Id$
 *
 */

/**
 * User hooks for civicrm module
 *
 * Note that we ignore the edit field and do not follow the drupal protocol
 * of extracting values from the edit field. We extract the fields directly
 * from $_POST. This is because the underlying form package that we use
 * (HTML_QuickForm) does the form value extraction and validation.
 *
 * @abstractparam array  $edit     The array of form values submitted by the user.
 *
 * @param object $user     The user object on which the operation is being performed.
 * @param object $category The active category of user information being edited.
 *
 * @return mixed           depends on the operation being performed
 */

/**
 * Implementation of hook_user_login (D7)
 */
function civicrm_user_login(&$edit, $user) {
  if (!civicrm_initialize()) {
    return;
  }
  return CRM_Core_BAO_UFMatch::synchronize($user, FALSE, 'Drupal',
    civicrm_get_ctype('Individual')
  );
}

/**
 * Implementation of hook_user_insert
 */
function civicrm_user_insert(&$edit, &$user, $category = NULL) {
  if (!civicrm_initialize()) {
    return;
  }

  $config = CRM_Core_Config::singleton();
  if ($config->inCiviCRM) {
    return;
  }

  // did civicrm generate this page, or is it via a user hook?
  if (civicrm_on_user_page()) {
    civicrm_register_data($edit, $user, $category, FALSE);
  }
  else {
    CRM_Core_BAO_UFMatch::synchronize($user, FALSE, 'Drupal',
      civicrm_get_ctype('Individual')
    );
  }
  return;
}

/**
 * Implementation of hook_user_update
 */
function civicrm_user_update(&$edit, &$user, $category) {
  if (!civicrm_initialize()) {
    return;
  }
  // this always comes in via user hook
  // in D7 we dont know if the email has changed, so we go ahead and update
  if (isset($edit['mail']) && !empty($edit['mail'])) {
    $contactID = CRM_Core_BAO_UFMatch::getContactId($user->uid);
    // cant find the contactID, so lets skip
    if (!$contactID) {
      return;
    }
    $contactEmail = CRM_Contact_BAO_Contact::getPrimaryEmail($contactID);
    $userEmail = trim($edit['mail']);
    if ($contactEmail != $userEmail) {
      CRM_Core_BAO_UFMatch::updateContactEmail($contactID, $userEmail);
    }

    // reset navigation on user role change
    $editRoles     = array_keys(CRM_Utils_Array::value('roles', $edit, array()));
    $orginRoles    = array_keys($user->original->roles);
    $editRoleDiff  = array_diff($editRoles, $orginRoles);
    $orginRoleDiff = array_diff($orginRoles, $editRoles);
    if (!empty($editRoleDiff) || !empty($orginRoleDiff)) {
      CRM_Core_BAO_Navigation::resetNavigation($contactID);
    }
  }
}

/**
 * Implementation of hook_user_delete
 */
function civicrm_user_delete($account) {
  if (!civicrm_initialize()) {
    return;
  }
  CRM_Core_BAO_UFMatch::deleteUser($account->uid);
}

/**
 * Implementation of hook_user_categories
 */
function civicrm_user_categories() {
  if (!civicrm_initialize()) {
    return;
  }

  $allUFGroups = CRM_Core_BAO_UFGroup::getModuleUFGroup('User Account', 0, TRUE, CRM_Core_Permission::VIEW, array('id', 'name', 'title', 'is_active'));
  $ufGroups = array();

  $weight = 100;
  foreach ($allUFGroups as $key => $value) {
    if ($value['is_active']) {
      $ufGroups[] = array(
        'name' => $value['name'],
        'title' => $value['name'],
        'weight' => $weight,
        'access callback' => '_civicrm_categories_access',
        'access arguments' => array("$key"),
      );
      $weight += 10;
    }
  }

  return $ufGroups;
}

/**
 * Implementation of hook_user_view
 *
 * @todo I suspect that some of the stuff done in the old form_alter handler
 *       should live here instead under D7
 */
function civicrm_user_view($user, $view_mode) {
  if (!civicrm_initialize()) {
    return;
  }
  $userID = CRM_Core_BAO_UFMatch::getContactId($user->uid);
  if ($userID) {
    // make sure user has permission to view the record
    $contactURL = NULL;
    $civiPerm = CRM_Contact_BAO_Contact_Permission::allow($userID);
    if (CRM_Core_Permission::check('access CiviCRM') && $civiPerm) {
      $contactURL =
  '<span class="user-page-link" id="user-page-contact" ><span class="user-page-bullet">&raquo;</span> ' .
  l(ts("View Contact Record"),
    'civicrm/contact/view',
    array('query' => array('reset' => 1, 'cid' => $userID ))
    ) .
  '</span>';
    }

    if (CRM_Core_Permission::check('access Contact Dashboard') && $civiPerm) {
      if (!empty($contactURL)) {
        $contactURL .= '<br/>';
      }
      $contactURL .=
  '<span class="user-page-link" id="user-page-dashboard" ><span class="user-page-bullet">&raquo;</span> ' .
  l(ts("View Contact Dashboard"),
    'civicrm/user',
    array('query' => array('reset' => 1, 'id' => $userID ))
    ) .
  '</span>';
    }

    $ctype = CRM_Core_DAO::getFieldValue('CRM_Contact_DAO_Contact', $userID, 'contact_type');

    $ufGroups = CRM_Core_BAO_UFGroup::getModuleUFGroup('User Account', 0, TRUE);
    $weight = 100;
    foreach ($ufGroups as $id => $title) {

      $fieldType = CRM_Core_BAO_UFField::getProfileType($id);
      if (CRM_Contact_BAO_ContactType::isaSubType($fieldType)) {
        $fieldType = CRM_Contact_BAO_ContactType::getBasicType($fieldType);
      }
      if (($fieldType != 'Contact') && ($fieldType != $ctype)) {
        continue;
      }
      $page = new CRM_Profile_Page_Dynamic($userID, $id, NULL, TRUE);
      $pageContent = $page->run();

      // CRM-3537: profile edit link
      $editURL = '';
      if (user_edit_access($user)) {
        $editURL = '<span class="user-page-link" id="user-page-profile-' . substr($title['title'], 0, 3) . '" ><span class="user-page-bullet">&raquo;</span> ' . l(ts("Edit %1", array(1 => $title['title'])),
          "user/{$user->uid}/edit/" . $title['title']
        ) . '</span>';
      }

      if ($pageContent) {
        $user->content[$title['title']] = array(
          '#title' => $title['title'],
          '#type' => 'user_profile_category',
          '#weight' => $weight,
        );
        $user->content[$title['title']][$title['title']] = array(
          '#type' => 'user_profile_item',
          '#title' => NULL,
          '#value' => $pageContent . $editURL,
          '#markup' => $pageContent . $editURL,
        );

        $weight += 10;
      }
    }

    if ($contactURL) {
      $user->content['urls'] = array(
        '#markup' => $contactURL,
        '#weight' => $weight,
      );
    }
  }
  return;
}

/**
 * Implementation of hook_user_logout
 *
 */
function civicrm_user_logout($account) {
  if (!civicrm_initialize()) {
    return;
  }
  $session = CRM_Core_Session::singleton();
  $session->reset();
}

/**
 * Validation functions so CiviCRM can get at these items
 */
function civicrm_validate_user_forms($form, &$form_state) {
  if (!civicrm_initialize()) {
    return;
  }
  // we ignore this
  $edit = array();
  // this as well
  $category = FALSE;
  global $user;
  civicrm_validate_data($edit, $user, $category);
}

