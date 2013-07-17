drop procedure if exists sp_check_pin;

/*

  Check a pin is valid, and return an approprite unlock text if it is.
  If the PIN is found and is set to enroll, register the last card read 
  (if within timeout).
  If registation is successfull, cancel the PIN.
  
  In all cases, log an entry in the access log.
*/

DELIMITER //
CREATE PROCEDURE sp_check_pin
(
   IN  pin          varchar(12),
   OUT unlock_text  varchar(95),
   OUT username     varchar(50),
   OUT err          varchar(100)
)
SQL SECURITY DEFINER
BEGIN
  declare ck_exists int;
  declare access_granted int;
  
  declare p_pin_id int;
  declare p_unlock_text varchar (95);
  declare p_expiry timestamp;
  declare p_state int;
  declare p_member_id int;
  
  declare l_rfid_serial varchar(50);
  declare l_access_result int;
  declare l_access_time timestamp;
  
  set ck_exists = 0;
  set access_granted = 0;
  set p_member_id = NULL;
 
  main: begin  
  
    -- First, see if the pin is known
    select count(*) into ck_exists
    from pins p
    where p.pin = pin
      and p.state in (10, 40); -- active, enroll
      
    if (ck_exists != 1) then
      set err = "PIN not found";
      set unlock_text = "Access Denied";
      leave main;
    end if;
  
    select
      p.pin_id,
      m.unlock_text,
      p.expiry,
      p.state,
      p.member_id,
      coalesce(m.username, '<unknown>')
    into
      p_pin_id,
      p_unlock_text,
      p_expiry,
      p_state,
      p_member_id,
      username
    from pins p 
    inner join members m on p.member_id = m.member_id
    where p.pin = pin
      and p.state in (10, 40); -- active, enroll

    -- check pin has not expired
    if (p_expiry > sysdate()) then
      set err = "PIN expired";
      set unlock_text = "Access Denied";
      
      -- now update to expired
      update pins
      set state = 20 -- expired
      where pin_id = p_pin_id;
      
      leave main;      
    end if;
    
    -- Check for an enroll pin (to register a card)
    if (
          (p_state = 40) and -- enroll
          (p_member_id is not null)
        )
    then      
      select 
        l.access_time,
        l.rfid_serial,
        l.access_result
      into
        l_access_time,
        l_rfid_serial,
        l_access_result
      from access_log l 
      where l.rfid_serial is not null 
      order by access_id 
      desc limit 1;
      
      -- check the card is suitable (not unknown type)
      if (l_rfid_serial = 'Unknown Card Type') then
        set err = 'Unknown Card Type';
        set unlock_text = concat('Unlock:', coalesce(p_unlock_text, 'Welcome'));
        set access_granted = 1;
        leave main;
      end if;        
      
      -- check the card is suitable (not already registered)
      select count(*) into ck_exists
      from rfid_tags t
      where t.rfid_serial = l_rfid_serial;
      
      -- Card not suitable, but PIN was valid, so unlock
      if (ck_exists > 0) then
        set err = "Card already registerd!";
        set unlock_text = concat('Unlock:', coalesce(p_unlock_text, 'Welcome'));
        set access_granted = 1;
        leave main;
      end if;
      
      -- Check time hasn't expired
      if ((unix_timestamp(sysdate()) - unix_timestamp(l_access_time)) > 60) then
        set err = "Time expired!";
        set unlock_text = concat('Unlock:',  coalesce(p_unlock_text, 'Welcome'));
        set access_granted = 1;
        leave main;
      end if;
      
      -- To get this far, everything should be in order, so register the card
      call sp_add_card(p_member_id, l_rfid_serial, err);
      if (err is null) then
        set unlock_text = concat('Unlock:', 'Card registered, PIN cancelled!');
        set access_granted = 1;
        
        -- Now a card has been registered using it, cancel the PIN
        update pins
        set state = 30 -- cancelled
        where pin_id = p_pin_id;
        
        leave main;
      else
        -- Somethings gone wrong - the card couldn't be registered
        set unlock_text = concat('Unlock:', 'Card registration failed.');
        set access_granted = 1;
        set err = "Failed to register card!";
        leave main;
      end if;
    else
      -- regualar, non-expired PIN
      set unlock_text = concat('Unlock:',  coalesce(p_unlock_text, 'Welcome'));
      set access_granted = 1;
      set err = null;
      leave main;
    end if;
    
  end main;
  
  if (access_granted = 1) then
    insert into access_log (rfid_serial, pin, access_result, member_id)
    values (null, pin, 20, p_member_id); -- granted
  else
    insert into access_log (rfid_serial, pin, access_result, member_id)
    values (null, pin, 10, p_member_id); -- denied
  end if;

END //
DELIMITER ;
