drop procedure if exists sp_check_rfid;

/*
  Check an rfid serial is valid, and return an approprite unlock text if it is.
  Then log an entry in the access log (either granted or denied).
*/

DELIMITER //
CREATE PROCEDURE sp_check_rfid
(
   IN  rfid_serial  varchar(50),
   OUT unlock_text  varchar(95),
   OUT handle       varchar(100),
   OUT last_seen    varchar(100),
   OUT err          varchar(100)
)
SQL SECURITY DEFINER
BEGIN
  declare ck_exists int;
  declare access_granted int;
  declare r_state int;
  declare member_id int;

  main: begin  
    set access_granted = 0;
    
    -- First, check the card is suitable (not unknown type)
    if (rfid_serial = 'Unknown Card Type') then
      set err = 'Unknown Card Type';
      set unlock_text = 'Unknown Card Type';
      leave main;
    end if;   

    -- See if the serial is known
    select count(*) into ck_exists
    from members m 
    inner join rfid_tags r on r.member_id = m.member_id 
    where r.rfid_serial = rfid_serial;
      
    if (ck_exists = 0) then
      set err = "RFID serial not found";
      set unlock_text = "Access Denied";
      leave main;
    end if;

    select 
      m.member_id,
      concat('Unlock:',  coalesce(m.unlock_text, 'Welcome')),
      coalesce(m.handle, m.username, m.name, '<unknown>'),
      r.state
    into
      member_id,
      unlock_text,
      handle,
      r_state
    from members m 
    inner join rfid_tags r on r.member_id = m.member_id 
    where r.state = 10 -- STATE_ACTIVE
      and r.rfid_serial = rfid_serial
    order by state limit 1;

    if (r_state != 10) then -- STATE_ACTIVE
      set err = "RFID serial not active";
      set unlock_text = "Access Denied";
      leave main;
    end if;
    
    set access_granted = 1;
  end main;
  
  -- Get last seen text from access log
  call sp_last_seen(member_id, last_seen);

  -- add entry to access log
  if (access_granted = 1) then
    insert into access_log (rfid_serial, pin, access_result)
    values (rfid_serial, null, 20); -- granted
  else
    insert into access_log (rfid_serial, pin, access_result)
    values (rfid_serial, null, 10); -- denied
  end if;

END //
DELIMITER ;
